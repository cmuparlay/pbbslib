#include "sequence.h"
#include "get_time.h"
#include "strings/string_basics.h"
#include "parse_command_line.h"
#include "group_by.h"

using namespace pbbs;

int main (int argc, char *argv[]) {
  commandLine P(argc, argv, "[-r <rounds>] infile");
  int rounds = P.getOptionIntValue("-r", 1);
  char* filename = P.getArgument(0);
  timer t("word counts", true);

  auto str = pbbs::char_range_from_file(filename);
  t.next("read file");

  auto is_line_break = [&] (char a) {return a == '\n';};
  auto is_space = [&] (char a) {return a == '\n' || a == '\t' || a == ' ';};

  size_t lines, words;
  for (int i=0; i < rounds; i++) {
    lines = count_if(str, is_line_break);
    auto x = seq(str.size(), [&] (size_t i) {
	return (i == 0 || is_space(str[i-1])) && !is_space(str[i]);});
    words = count(x, true);
    t.next("calculate counts");
  }
  
  cout << "  " << lines << "  " << words << " "
       << str.size() << " " << filename << endl;
}

