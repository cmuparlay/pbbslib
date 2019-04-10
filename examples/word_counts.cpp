#include "sequence.h"
#include "get_time.h"
#include "strings/string_basics.h"
#include "parse_command_line.h"
#include "group_by.h"

using namespace std;
using namespace pbbs;

timer t("word counts", true);

auto build_index(sequence<char> const &str) {
  auto is_space = [&] (char a) {return a == ' ' || a == '\t';};

  // remove punctuation and convert to lower case
  sequence<char> cleanstr = map(str, [&] (char a) -> char {
      return ispunct(a) ? ' ' : tolower(a); });

  // split into lines
  auto lines = split(cleanstr, "\n\r");
  cout << "num lines: " << lines.size() << endl;
  
  // generate sequence of sequences of (token, line_number) pairs
  auto pairs = tabulate(lines.size(), [&] (size_t i) {
      return map(tokenize(lines[i], is_space), [&] (char* w) {
	  return make_pair(w, i);
	});});

  // flatten and group line numbers by tokens
  return group_by(flatten(pairs));
}
  
int main (int argc, char *argv[]) {
  commandLine P(argc, argv, "[-r <rounds>] filename");
  int rounds = P.getOptionIntValue("-r", 3);
  char* filename = P.getArgument(0);
  
  pbbs::sequence<char> str = pbbs::char_seq_from_file(filename);
  t.next("read file");
  
  for (int i=0; i < rounds ; i++) {
    auto r = build_index(str);
    cout << "num words: " << r.size() << endl;
    t.next("build index");
  }
  
}
