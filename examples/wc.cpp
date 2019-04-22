// Word Count.
// Prints the number of lines, number of space separated words, and number of
// characters to stdout.

#include "sequence.h"
#include "get_time.h"
#include "strings/string_basics.h"
#include "parse_command_line.h"
#include "group_by.h"

using namespace pbbs;

template <class Seq>
std::tuple<size_t,size_t,size_t> wc(Seq const &s) {
  using P = std::pair<size_t,size_t>;

  // Create a delayed sequence of pairs of integers:
  // the first is 1 if it is line break, 0 otherwise;
  // the second is 1 if the start of a word, 0 otherwise.
  auto x = dseq(s.size(), [&] (size_t i) {
      auto is_space = [] (char a) {
	return a == '\n' || a == '\t' || a == ' ';};
      bool is_line_break = s[i] == '\n';
      bool word_start = ((i == 0 || is_space(s[i-1])) &&
			 !is_space(s[i]));
      return P(is_line_break, word_start);
    });

  // Reduce summing the pairs to get total line breaks and words.
  // This is faster than summing them separately since that would
  // require going over the input sequence twice.
  auto r = reduce(x, pair_monoid(addm<size_t>(),addm<size_t>())); 

  return std::make_tuple(r.first, r.second, s.size());
}

int main (int argc, char *argv[]) {
  commandLine P(argc, argv, "[-r <rounds>] infile");
  int rounds = P.getOptionIntValue("-r", 1);
  char* filename = P.getArgument(0);
  timer t("word counts", true);

  auto str = pbbs::char_range_from_file(filename);
  t.next("read file");

  size_t lines, words, bytes;
  for (int i=0; i < rounds; i++) {
    std::tie(lines, words, bytes) = wc(str);
    t.next("calculate counts");
  }
  
  cout << "  " << lines << "  " << words << " "
       << bytes << " " << filename << endl;
}

