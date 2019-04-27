#include "sequence.h"
#include "random.h"
#include "get_time.h"
#include "parse_command_line.h"

using namespace std;
using namespace pbbs;

// Maximum Contiguous Subsequence Sum

// 10x improvement by using delayed sequence
template <class Seq>
typename Seq::value_type mcss(Seq const &A) {
  using T = typename Seq::value_type;
  using TT = tuple<T,T,T,T>;
  auto f = [&] (TT a, TT b) {
    T aa, pa, sa, ta, ab, pb, sb, tb;
    tie(aa, pa, sa, ta) = a;
    tie(ab, pb, sb, tb) = b;
    return TT(max(aa,max(ab,sa+pb)),
	      max(pa, ta+pb),
	      max(sa + ab, sb),
	      ta + tb);
  };
  auto S = delayed_seq<TT>(A.size(), [&] (size_t i) {
      return TT(A[i],A[i],A[i],A[i]);});
  TT r = reduce(S, make_monoid(f, TT(0,0,0,0)));
  return get<0>(r);
}

int main (int argc, char *argv[]) {
  commandLine P(argc, argv, "[-r <rounds>] [-n <size>]");
  int rounds = P.getOptionIntValue("-r", 3);
  size_t n = 100000000;
  n = P.getOptionLongValue("-n", 1);
  timer t("MCSS");
  using T = double;

  pbbs::random r(0);
  sequence<T> A(n, [&] (size_t i) {return (T) (r[i]%n - n/2);});
  T result;
  for (int i=0; i < rounds; i++) {
    result = mcss(A);
    t.next("Total");
  }
  cout << result << endl;
}
