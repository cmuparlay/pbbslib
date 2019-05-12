// Build_Index.
// Builds an index from a file which maps each word to all the lines
// it appears in.
// It outputs to a file given by "-o <outfile>".  If no file is given,
// it just prints the number of distinct words to stdout.
// The outfile file has one line per word, with the word appearing
// first and then all the line numbers the word appears in.  The words
// are in alphabetical order, and the line numbers are in integer
// order, all in ascii.

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

auto build_index(sequence<char> const &str, bool verbose) -> index_type {
  timer t("build_index", verbose); // set to true to print times for each step
  auto is_line_break = [&] (char a) {return a == '\n' || a == '\r';};
  auto is_space = [&] (char a) {return a == ' ' || a == '\t';};
  
  // remove punctuation and convert to lower case
  sequence<char> cleanstr = map(str, [&] (char a) -> char {
      return isspace(a) ? a : isalpha(a) ? tolower(a) : ' ';});
  t.next("clean");
  
  // split into lines
  auto lines = split(cleanstr, is_line_break);
  t.next("split");
  
  // generate sequence of sequences of (token, line_number) pairs
  // tokens are strings separated by spaces.
  auto pairs = tabulate(lines.size(), [&] (size_t i) {
      return dmap(tokens(lines[i], is_space), [=] (sequence<char> s) {
	  return make_pair(s, i);});
      });
  t.next("tokens");

  // flatten the sequence
  auto flat_pairs = flatten(pairs);
  t.next("flatten");
      
  // group line numbers by tokens
  return group_by(flat_pairs);
}

// converts an index into an ascii character sequence ready for output
sequence<char> index_to_char_seq(index_type const &idx) {

  // print line numbers separated by spaces for a singe word
  auto linelist = [] (auto &A) {
    return flatten(tabulate(2 * A.size(), [&] (size_t i) {
	  if (i & 1) return to_char_seq(A[i/2]);
	  return singleton(' ');
	}));
  };

  // for each entry, print word followed by list of lines it is in
  return flatten(map(idx, [&] (auto& entry) {
	sequence<sequence<char>>&& A = {entry.first,
					linelist(entry.second),
					singleton('\n')};
	return flatten(A);}));
}

int main (int argc, char *argv[]) {
  commandLine P(argc, argv, "[-r <rounds>] [-o <outfile>] infile");
  int rounds = P.getOptionIntValue("-r", 1);
  bool verbose = P.getOption("-v");
  std::string outfile = P.getOptionValue("-o", "");
  char* filename = P.getArgument(0);
  timer idx_timer("build_index", verbose);
  auto str = pbbs::char_range_from_file(filename);
  idx_timer.next("read file");
  index_type idx;

  // resereve 5 x the number of bytes of the string for the memory allocator
  // not needed, but speeds up the first run
  pbbs::allocator_reserve(str.size()*5);

  idx_timer.next("reserve space");

  idx_timer.start();
  for (int i=0; i < rounds ; i++) {
    idx = build_index(str, verbose);
    idx_timer.next("build index");
  }

  cout << (idx[0].second)[2] << endl;

  if (outfile.size() > 0) {
    auto out_str = index_to_char_seq(idx);
    idx_timer.next("generate output string");

    char_seq_to_file(out_str, outfile);
    idx_timer.next("write file");
  } else {
    cout << "number of distinct words: " << idx.size() << endl;
  }
}
