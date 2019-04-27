#include "sequence.h"
#include "get_time.h"
#include "strings/string_basics.h"
#include "parse_command_line.h"
#include "group_by.h"

using namespace pbbs;

int main (int argc, char *argv[]) {
  commandLine P(argc, argv, "[-r <rounds>] search_string infile");
  int rounds = P.getOptionIntValue("-r", 1);
  auto search_str = to_sequence(std::string(P.getArgument(1)));
  char* filename = P.getArgument(0);

  timer t("grep", true);
  
  pbbs::range<char*> str = pbbs::char_range_from_file(filename);
  t.next("read file");
  sequence<char> out_str;

  for (int i=0; i < rounds; i++) {
    // auto foo = pack_index<size_t>(seq(str.size(), [&] (size_t i) {
    // 	  return str[i] == '\n';}));
    // size_t l = str.size() - search_str.size() + 1;
    // auto bar = pack_index<size_t>(seq(l, [&] (size_t i) {
    // 	  size_t j;
    // 	  for (j=0; j < search_str.size(); j++)
    // 	    if (str[i+j] != search_str[j]) break;
    // 	  return (j == search_str.size());
    // 	}));
    auto is_line_break = [&] (char a) {return a == '\n';};
    auto cr = singleton('\n');
    auto lines = filter(split(str, is_line_break), [&] (auto const &s) {
    	return search(s, search_str) < s.size();});
    out_str = flatten(tabulate(lines.size()*2, [&] (size_t i) {
    	  return (i & 1) ? cr : std::move(lines[i/2]);}));
    t.next("do work");
  }
  cout << out_str;
}

