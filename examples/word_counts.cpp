#include "sequence.h"
#include "random.h"
#include "get_time.h"
#include "strings/string_basics.h"
#include "parse_command_line.h"

using namespace std;
using namespace pbbs;

int main (int argc, char *argv[]) {
  commandLine P(argc, argv, "[-r <rounds>] filename");
  int rounds = P.getOptionIntValue("-r", 3);
  char* filename = P.getArgument(0);
  timer t("word counts", true);

  pbbs::sequence<char> str = pbbs::char_seq_from_file(filename);
  t.next("read file");
  for (int i=0; i < rounds ; i++) {
    pbbs::sequence<char*> words = pbbs::tokenize(str, is_space);
    t.next("tokenize");
    auto strless = [&] (char* a, char* b) {return strcmp(a,b) < 0;};
    words = pbbs::sort(words, strless);
    t.next("sort");
  }
  
}
