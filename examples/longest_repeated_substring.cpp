#include "sequence.h"
#include "get_time.h"
#include "strings/string_basics.h"
#include "strings/suffix_array.h"
#include "strings/lcp.h"
#include "parse_command_line.h"

using namespace pbbs;

// returns
//  1) the length of the longest match
//  2) start of the first string in s
//  3) start of the second string in s
tuple<uint,uint,uint> lrs(sequence<uchar> const &s) {
  timer t("lrs", true);

  sequence<uint> sa = suffix_array<uint>(s);
  t.next("suffix array");

  sequence<uint> lcps = lcp(s, sa);
  t.next("lcps");

  size_t idx = max_element(lcps, std::less<uint>());
  t.next("max element");
    
  return tuple<uint,uint,uint>(lcps[idx],sa[idx],sa[idx+1]);
}

int main (int argc, char *argv[]) {
  commandLine P(argc, argv, "[-r <rounds>] infile");
  int rounds = P.getOptionIntValue("-r", 1);
  int output = P.getOption("-o");
  char* filename = P.getArgument(0);
  timer t("lrs", true);

  auto istr = pbbs::char_range_from_file(filename);
  sequence<uchar> str(istr.size(), [&] (size_t i) -> uchar {
      return istr[i];});
  t.next("read file");

  size_t len=0, match1, match2;
  
  for (int i=0; i < rounds; i++) {
    std::tie(len, match1, match2) = lrs(str);
    t.next("calculate longest repeated substring");
  }
  
  cout << "Longest match length = " << len
       << " at " << match1 << " and " << match2 << endl;

  if (output) {
    cout << (sequence<char>(len, [&] (size_t i) -> char {
	  return str[match1 + i];})) << endl;
  }
}
