#include "sequence.h"
#include "get_time.h"
#include "strings/string_basics.h"
#include "parse_command_line.h"
#include "group_by.h"
using namespace std;
using namespace pbbs;

timer idx_timer("build_index");

// an index consists of a sequence of pairs each consisting of
//     a character sequence (the word)
//     an integer sequences (the line numbers it appears in)
using index_type = sequence<pair<sequence<char>,sequence<size_t>>>;

auto build_index(sequence<char> const &str) -> index_type {
  auto is_line_break = [&] (char a) {return a == '\n' || a == '\r';};
  auto is_space = [&] (char a) {return a == ' ' || a == '\t';};
  
  // remove punctuation and convert to lower case
  sequence<char> cleanstr = map(str, [&] (char a) -> char {
      return isspace(a) ? a : isalpha(a) ? tolower(a) : ' ';});

  // split into lines
  auto lines = split(cleanstr, is_line_break);

  // generate sequence of sequences of (token, line_number) pairs
  auto pairs = tabulate(lines.size(), [&] (size_t i) {
      return map(tokens(lines[i], is_space), [&] (sequence<char> &w) {
	  return make_pair(w, i);
	});});
  
  // flatten and group line numbers by tokens
  return group_by(flatten(pairs));
}

auto index_to_char_seq(index_type const &idx) {
  return flatten(map(idx, [&] (auto& entry) {
	sequence<sequence<char>>&& A = {entry.first,
					singleton(' '),
					to_char_seq(entry.second),
					singleton('\n')};
	return flatten(A);}));
}

int main (int argc, char *argv[]) {
  commandLine P(argc, argv, "[-r <rounds>] [-o <outfile>] infile");
  int rounds = P.getOptionIntValue("-r", 1);
  std::string outfile = P.getOptionValue("-o", "");
  char* filename = P.getArgument(0);
  idx_timer.start();
  
  pbbs::sequence<char> str = pbbs::char_seq_from_file(filename);
  idx_timer.next("read file");
  index_type idx;

  for (int i=0; i < rounds ; i++) {
    idx = build_index(str);
    idx_timer.next("build index");
  }

  cout << "number of distinct words: " << idx.size() << endl;

  if (outfile.size() > 0) {
    auto out_str = index_to_char_seq(idx);
    idx_timer.next("generate output string");

    char_seq_to_file(out_str, outfile);
    idx_timer.next("write file");
  }
}
