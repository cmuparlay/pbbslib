#include "sequence.h"
#include "get_time.h"
#include "strings/string_basics.h"
#include "parse_command_line.h"
#include "group_by.h"

using namespace std;
using namespace pbbs;

// an index consists of a sequence of pairs each consisting of
//     a character sequence (the word)
//     an integer sequences (the line numbers it appears in)
using index_type = sequence<pair<sequence<char>,sequence<size_t>>>;

template <class Seq, class UnaryPred>
sequence<sequence<char>> split2(Seq const &S, UnaryPred const &is_space) {
  size_t n = S.size();

  auto X = sequence<bool>(n, [&] (size_t i) {
      return is_space(S[i]);});
  sequence<long> Locations = pbbs::pack_index<long>(X);
  size_t m = Locations.size();
  
  return tabulate(m + 1, [&] (size_t i) -> sequence<char> {
      size_t start = (i==0) ? 0 : Locations[i-1] + 1;
      size_t end = (i==m) ? n : Locations[i];
      return sequence<char>(S.slice(start, end));});
}

template <class Seq, class UnaryPred>
sequence<sequence<char>> tokens2(Seq const &S, UnaryPred const &is_space) {
  size_t n = S.size();
  if (n==0) return sequence<sequence<char>>();
  sequence<bool> Flags(n+1);
  parallel_for(1, n, [&] (long i) {
      Flags[i] = is_space(S[i-1]) != is_space(S[i]);
    }, 10000);
  Flags[0] = !is_space(S[0]);
  Flags[n] = !is_space(S[n-1]);

  sequence<long> Locations = pbbs::pack_index<long>(Flags);
  
  return tabulate(Locations.size()/2, [&] (size_t i) {
      return sequence<char>(S.slice(Locations[2*i], Locations[2*i+1]));});
}

auto build_index(sequence<char> const &str) -> index_type {
  auto is_space = [&] (char a) {return a == ' ' || a == '\t';};
  auto is_line = [&] (char a) {return a == '\n' || a == '\r';};
  timer t;
  cout << "num chars: " << str.size() << endl;
  
  // remove punctuation and convert to lower case
  sequence<char> cleanstr = map(str, [&] (char a) -> char {
      return ispunct(a) ? ' ' : tolower(a); });
  t.next("clean");

  // split into lines
  //auto lines = split2(cleanstr, "\n\r");
  auto lines = split2(cleanstr, is_line);
  cout << "num lines: " << lines.size() << endl;
  t.next("split");

        //auto words = tokens2(lines[i], is_space);
      //return tabulate(words.size(), [&] (size_t j) {

  // generate sequence of sequences of (token, line_number) pairs
  auto&& pairs = tabulate(lines.size(), [&] (size_t i) {
      return map(tokens2(lines[i], is_space), [&] (sequence<char> &w) {
	  return make_pair(std::move(w), i);
	});});
  t.next("pairs");
  cout << "num nested pairs: " << pairs.size() << endl;
  
  auto&& f = flatten(pairs); 
  t.next("flatten");
  cout << "num  pairs: " << f.size() << endl;
  
  // flatten and group line numbers by tokens
  return group_by(f);
}

void write_index_to_file(index_type const &idx, string outfile) {
  timer t("write");

  auto&& entries = map(idx, [&] (auto& entry) {
      sequence<sequence<char>>&& A = {entry.first,
				      singleton(' '),
				      to_char_seq(entry.second),
				      singleton('\n')};
      return flatten(A);});

  auto rstr = flatten(entries); //std::move(entries));
  t.next("gen out string");
  char_seq_to_file(rstr, outfile);
  t.next("write file");
}

int main (int argc, char *argv[]) {
  commandLine P(argc, argv, "[-r <rounds>] filename");
  int rounds = P.getOptionIntValue("-r", 3);
  std::string outfile = P.getOptionValue("-o", "");
  
  //sequence<sequence<char>> foo(20,[&] (int i) {return sequence<char>(i,'a');});
  //cout << foo[0].size() << endl;
  //for (int i=1; i < 20; i++) cout << foo[i].size() << ", " << foo[i][0] << endl;

  char* filename = P.getArgument(0);
   timer t("word counts", true);
  
   pbbs::sequence<char> str = pbbs::char_seq_from_file(filename);
   t.next("read file");
   index_type idx;

   for (int i=0; i < rounds ; i++) {
     idx = build_index(str);
     t.next("build index");
   }
   // thisisaverylongword andthisisanotherone

  // size_t x = reduce(tabulate(idx.size(), [&] (size_t i) -> size_t {
  // 	return (strlen(idx[i].first) < 11) ? (idx[i].second).size() : 0;}),
  //   addm<size_t>());

  // size_t y = reduce(tabulate(idx.size(), [&] (size_t i) -> size_t {
  // 	return (idx[i].second).size();}),addm<size_t>());
  // cout << "small count : " << x << ", " << y << endl;
  
  cout << "num entries: " << idx.size() << endl;
   // thisisaverylongword andthisisanotherone
  
  if (outfile.size() > 0) 
    write_index_to_file(idx, outfile);
  
}
