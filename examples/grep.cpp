#include "sequence.h"
#include "get_time.h"
#include "strings/string_basics.h"
#include "parse_command_line.h"
#include "group_by.h"

using namespace pbbs;

int main (int argc, char *argv[]) {
  commandLine P(argc, argv, "[-r <rounds>] search_string infile");
  int rounds = P.getOptionIntValue("-r", 1);
  std::string sstr(P.getArgument(1));
  sequence<char> search_str(sstr.size(), [&] (size_t i) {
      return sstr[i];});
  char* filename = P.getArgument(0);

  timer t("grep", true);
  
  pbbs::sequence<char> str = pbbs::char_seq_from_file(filename);
  t.next("read file");
  sequence<char> out_str;

  for (int i=0; i < rounds; i++) {
    auto is_line_break = [&] (char a) {return a == '\n';};
    auto cr = singleton('\n');
    auto lines = filter(split(str, is_line_break), [&] (auto s) {
	return search(s, search_str) < s.size();});
    out_str = flatten(tabulate(lines.size()*2, [&] (size_t i) {
	  return (i & 1) ? cr : lines[i/2];}));
    t.next("do work");
  }
  cout << out_str;
}

