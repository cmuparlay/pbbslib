#include "sequence.h"
//#include "suffix_tree.h"
#include "suffix_array.h"
#include "lcp.h"
#include "string_basics.h"
#include <string>
#include <ctype.h>

using namespace pbbs;
using namespace std;

using Uchar = unsigned char;
using Uint = uint;
//using Node = node<Uint>;

int main (int argc, char *argv[]) {

  //string fname = "/home/guyb/tmp/pbbsbench/testData/sequenceData/data/wikisamp.xml";
  string fname = "/ssd0/text/HG18.fasta";
  sequence<char> teststr = char_seq_from_file(fname);
  //string teststr = "aabcabd";
  //string teststr = "bacacdaba";
  size_t n = teststr.size();
  cout << "n = " << n << endl;
  sequence<Uchar> a(n, [&] (size_t i) -> uchar {return teststr[i];});
  //cout << teststr << endl;
  //sequence<Node> R =
  timer t("total Suffix Tree", true);
  for (int i=0; i < 3; i++) {
    t.start();
    sequence<Uint> sa = suffix_array<Uint>(a);
    t.next("sa");
    sequence<Uint> lcp = LCP(a, sa);
    t.next("lcp");
    //suffix_tree<Uint> T(a);
  //t.next("");
  }
  //for (size_t i = 0; i < n; i++) cout << R[i].location << ", " << R[i].value << ", " << R[i].parent << endl;
}
