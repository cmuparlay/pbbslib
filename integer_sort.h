// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2010-2017 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once
#include <math.h>
#include <stdio.h>
#include <cstdint>
#include "utilities.h"
#include "counting_sort.h"
#include "quicksort.h"

namespace pbbs {

  constexpr size_t radix = 8;
  constexpr size_t max_buckets = 1 << radix;

  // a bottom up radix sort
  template <class Slice, class GetKey>
  void seq_radix_sort(Slice In, Slice Out, GetKey const &g,
		      size_t bits, bool inplace=true) {
    size_t n = In.size();
    if (n == 0) return;
    size_t counts[max_buckets+1];
    bool swapped = false;
    int bit_offset = 0;
    while (bits > 0) {
      size_t round_bits = std::min(radix, bits);
      size_t num_buckets = (1 << round_bits);
      size_t mask = num_buckets-1;
	auto get_key = [&] (size_t i) -> size_t {
	  return (g(In[i]) >> bit_offset) & mask;};
	seq_count_sort_(In, Out, delayed_seq<size_t>(n, get_key),
			counts, num_buckets);
	std::swap(In,Out);
      bits = bits - round_bits;
      bit_offset = bit_offset + round_bits;
      swapped = !swapped;
    }
    if ((inplace && swapped) || (!inplace && !swapped)) {
      for (size_t i=0; i < n; i++) 
	move_uninitialized(Out[i], In[i]);
    }
  }
  
  // a top down recursive radix sort
  // g extracts the integer keys from In
  // key_bits specifies how many bits there are left
  // if inplace is true, then result will be in In, otherwise in Out
  // In and Out cannot be the same
  template <typename Slice, typename Get_Key>
  void integer_sort_r(Slice In, Slice Out, Get_Key const &g, 
		      size_t key_bits, bool inplace) {
    size_t n = In.size();
    timer t;

    if (key_bits == 0) {
      if (!inplace)
	parallel_for(0, In.size(), [&] (size_t i) {Out[i] = In[i];});
      
    // for small inputs use sequential radix sort
    } else if (n < (1 << 15)) {
      seq_radix_sort(In, Out, g, key_bits, inplace);
    
    // few bits, just do a single parallel count sort
    } else if (key_bits <= radix) {
      t.start();
      size_t num_buckets = (1 << key_bits);
      size_t mask = num_buckets - 1;
      auto f = [&] (size_t i) {return g(In[i]) & mask;};
      auto get_bits = delayed_seq<size_t>(n, f);
      count_sort(In, Out, get_bits, num_buckets);
      if (inplace)
	parallel_for(0, n, [&] (size_t i) {
	    move_uninitialized(In[i], Out[i]);});
      
    // recursive case  
    } else {
      size_t bits = 8;
      size_t shift_bits = key_bits - bits;
      size_t buckets = (1 << bits);
      size_t mask = buckets - 1;
      auto f = [&] (size_t i) {return (g(In[i]) >> shift_bits) & mask;};
      auto get_bits = delayed_seq<size_t>(n, f);

      // divide into buckets
      sequence<size_t> offsets = count_sort(In, Out, get_bits, buckets);
      //if (n > 10000000) t.next("count sort");

      // recursively sort each bucket
      parallel_for(0, buckets, [&] (size_t i) {
	size_t start = offsets[i];
	size_t end = offsets[i+1];
	auto a = Out.slice(start, end);
	auto b = In.slice(start, end);
	integer_sort_r(a, b, g, shift_bits, !inplace);
	}, 1);
    }
  }

  // a top down recursive radix sort
  // g extracts the integer keys from In
  // result will be placed in out, 
  //    but if inplace is true, then result will be put back into In
  // both in and out can be modified
  // val_bits specifies how many bits there are in the key
  //    if set to 0, then a max is taken over the keys to determine
  template <typename IterIn, typename IterOut, typename Get_Key>
  void integer_sort_(slice_t<IterIn> In, slice_t<IterOut> Out,
		     Get_Key const &g, 
		     size_t key_bits=0, bool inplace=false) {
    using T = typename std::iterator_traits<IterIn>::value_type;
    if (In.begin() == Out.begin()) {
      cout << "in integer_sort : input and output must be different locations"
	   << endl;
      abort();}
    if (key_bits == 0) {
      using P = std::pair<size_t,size_t>;
      auto get_key = [&] (size_t i) -> P {
	size_t k =g(In[i]);
	return P(k,k);};
      auto keys = delayed_seq<P>(In.size(), get_key);
      size_t min_val, max_val;
      std::tie(min_val,max_val) = reduce(keys, minmaxm<size_t>());
      key_bits = log2_up(max_val - min_val + 1);
      if (min_val > max_val / 4) {
	auto h = [&] (T a) {return g(a) - min_val;};
	integer_sort_r(In.slice(), Out.slice(), h, key_bits, inplace);
	return;
      }
    }
    integer_sort_r(In.slice(), Out.slice(), g, key_bits, inplace);
  }

  template <typename T, typename Get_Key>
  void integer_sort(slice_t<T*> In,
		    Get_Key const &g,
		    size_t key_bits=0) {
    sequence<T> Tmp = sequence<T>::no_init(In.size());
    integer_sort_(In, Tmp.slice(), g, key_bits, true);
  }

}
