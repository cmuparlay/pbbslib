#pragma once
#include "sequence_ops.h"

template <class Seq, class Compare>
using T = typename Seq::value_type;
T& kth_smallest(Seq s, size_t k, Compare less, random r = default_random) {
  size_t n = s.size();
  T& pivot = s[r[0]%n];
  sequence<T> smaller = filter(seq, [&] (T a) {return less(a, pivot);});
  if (k < smaller.size())
    return kth_smallest(smaller, k, r.next());
  else {
    sequence<T> larger = filter(seq, [&] (T a) {return less(pivot, a);});
    if (k >= n - larger.size())
      return kth_smallest(larger, k - n + larger.size(), r.next());
    else return pivot;
  }
}
