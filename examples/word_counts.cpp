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

auto build_index(sequence<char> const &str) -> index_type {
  auto is_space = [&] (char a) {return a == ' ' || a == '\t';};
  auto is_line_break = [&] (char a) {return a == '\n' || a == '\r';};
  timer t;
  cout << "num chars: " << str.size() << endl;
  
  // remove punctuation and convert to lower case
  sequence<char> cleanstr = map(str, [&] (char a) -> char {
      return ispunct(a) ? ' ' : tolower(a); });
  t.next("clean");

  // split into lines
  auto lines = split(cleanstr, is_line_break);
  cout << "num lines: " << lines.size() << endl;
  t.next("split");

  // generate sequence of sequences of (token, line_number) pairs
  auto&& pairs = tabulate(lines.size(), [&] (size_t i) {
      return map(tokens(lines[i], is_space), [&] (sequence<char> &w) {
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

  auto rstr = flatten(entries); 
  t.next("gen out string");
  char_seq_to_file(rstr, outfile);
  t.next("write file");
}

int main (int argc, char *argv[]) {
  commandLine P(argc, argv, "[-r <rounds>] filename");
  int rounds = P.getOptionIntValue("-r", 3);
  std::string outfile = P.getOptionValue("-o", "");
  
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

   cout << "num entries: " << idx.size() << endl;
   // thisisaverylongword andthisisanotherone
  
  if (outfile.size() > 0) 
    write_index_to_file(idx, outfile);
  
}
