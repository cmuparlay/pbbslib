// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2010-2016 Guy Blelloch and the PBBS team
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
#include <stdio.h>
#include <math.h>
#include "utilities.h"
#include "sequence_ops.h"
#include "transpose.h"

// TODO
// Make sure works for inplace or not with regards to move_uninitialized

namespace pbbs {

  // the following parameters can be tuned
  constexpr const size_t SEQ_THRESHOLD = 8192;
  constexpr const size_t BUCKET_FACTOR = 32;
  constexpr const size_t LOW_BUCKET_FACTOR = 16;

  // count number in each bucket
  template <typename s_size_t, typename InSeq, typename KeySeq>
  void seq_count_(InSeq In, KeySeq Keys,
		  s_size_t* counts, size_t num_buckets) {
    size_t n = In.size();

    for (size_t i = 0; i < num_buckets; i++)
      counts[i] = 0;
    for (size_t j = 0; j < n; j++) {
      size_t k = Keys[j];
      if (k >= num_buckets) abort();
      counts[k]++;
    }
  }

  // write to destination, where offsets give start of each bucket
  template <typename s_size_t, typename InSeq, typename KeySeq>
  void seq_write_(InSeq In, typename InSeq::value_type* Out, KeySeq Keys,
		  s_size_t* offsets, size_t num_buckets) {
    for (size_t j = 0; j < In.size(); j++) {
      size_t k = offsets[Keys[j]]++;
      move_uninitialized(Out[k], In[j]);
    }
  }

  // write to destination, where offsets give end of each bucket
  template <typename s_size_t, typename InSeq, typename KeySeq>
  void seq_write_down_(InSeq In, typename InSeq::value_type* Out, KeySeq Keys,
		       s_size_t* offsets, size_t num_buckets) {
    for (long j = In.size()-1; j >= 0; j--) {
      long k = --offsets[Keys[j]];
      move_uninitialized(Out[k], In[j]);
    }
  }

  // Sequential counting sort internal
  template <typename s_size_t, typename InS, typename OutS, typename KeyS>
  void seq_count_sort_(InS In, OutS Out, KeyS Keys,
		       s_size_t* counts, size_t num_buckets) {

    // count size of each bucket
    seq_count_(In, Keys, counts, num_buckets);

    // generate offsets for buckets
    size_t s = 0;
    for (size_t i = 0; i < num_buckets; i++) {
      s += counts[i];
      counts[i] = s;
    }

    // send to destination
    seq_write_down_(In, Out.begin(), Keys, counts, num_buckets);
  }

  // Sequential counting sort
  template <typename InS, typename OutS, typename KeyS>
  sequence<size_t> seq_count_sort(InS& In, OutS& Out, KeyS& Keys,
				  size_t num_buckets) {
    sequence<size_t> counts(num_buckets+1);
    seq_count_sort_(In.slice(), Out.slice(), Keys.slice(),
		    counts.begin(), num_buckets);
    counts[num_buckets] = In.size();
    return counts;
  }

  // Parallel internal counting sort specialized to type for bucket counts
  template <typename s_size_t, 
	    typename InS, typename OutS, typename KeyS>
  sequence<size_t> count_sort_(InS& In, OutS& Out, KeyS& Keys,
			       size_t num_buckets) {
    timer t;
    t.start();
    using T = typename InS::value_type;
    size_t n = In.size();
    size_t num_threads = num_workers();

    // pad to 16 buckets to avoid false sharing (does not affect results)
    num_buckets = std::max(num_buckets, (size_t) 16);

    // if not given, then use heuristic to choose num_blocks
    size_t sqrt = (size_t) ceil(pow(n,0.5));
    size_t num_blocks = 
      (size_t) (n < (1<<24)) ? (sqrt/16) : ((n < (1<<28)) ? sqrt/2 : sqrt);
    if (2*num_blocks < num_threads) num_blocks *= 2;
    if (sizeof(T) <= 4) num_blocks = num_blocks/2;

    // if insufficient parallelism, sort sequentially
    if (n < SEQ_THRESHOLD || num_blocks == 1 || num_threads == 1) {
      return seq_count_sort(In,Out,Keys,num_buckets);}

    size_t block_size = ((n-1)/num_blocks) + 1;
    size_t m = num_blocks * num_buckets;
    
    s_size_t *counts = new_array_no_init<s_size_t>(m,1);
    if (n > 1000000000) t.next("head");

    // sort each block
    parallel_for(0, num_blocks,  [&] (size_t i) {
	s_size_t start = std::min(i * block_size, n);
	s_size_t end =  std::min(start + block_size, n);
	seq_count_(In.slice(start,end), Keys.slice(start,end),
		   counts + i*num_buckets, num_buckets);
      },1);

    if (n > 1000000000) t.next("count");

    sequence<size_t> bucket_offsets = sequence<size_t>::no_init(num_buckets+1);
    parallel_for(0, num_buckets, [&] (size_t i) {
	size_t v = 0;
	for (size_t j= 0; j < num_blocks; j++) 
	  v += counts[j*num_buckets + i];
	bucket_offsets[i] = v;
      }, 1 + 1024/num_blocks);
    bucket_offsets[num_buckets] = 0;
    size_t total = scan_inplace(bucket_offsets.slice(), addm<size_t>());
    if (total != n) abort();
    sequence<s_size_t> dest_offsets = sequence<s_size_t>::no_init(num_blocks*num_buckets);
    parallel_for(0, num_buckets, [&] (size_t i) {
	size_t v = bucket_offsets[i];
	size_t start = i * num_blocks;
	for (size_t j= 0; j < num_blocks; j++) {
	  dest_offsets[start+j] = v;
	  v += counts[j*num_buckets + i];
	}
      }, 1 + 1024/num_blocks);

    parallel_for(0, num_blocks, [&] (size_t i) {
	size_t start = i * num_buckets;
	for (size_t j= 0; j < num_buckets; j++)
	  counts[start+j] = dest_offsets[j*num_blocks + i];
      }, 1 + 1024/num_buckets);

    // transpose<s_size_t>(counts, dest_offsets.begin()).trans(num_blocks,
    // 							    num_buckets);
    // size_t sum = scan_inplace(dest_offsets, addm<s_size_t>());
    // if (sum != n) abort();
    // transpose<s_size_t>(dest_offsets.begin(), counts).trans(num_buckets,
    // 							    num_blocks);
    //if (n > 1000000000) t.next("scan");

    parallel_for(0, num_blocks,  [&] (size_t i) {
	s_size_t start = std::min(i * block_size, n);
	s_size_t end =  std::min(start + block_size, n);
	seq_write_(In.slice(start,end), Out.begin(),
		   Keys.slice(start,end),
		   counts + i*num_buckets, num_buckets);
      },1);

    //if (n > 1000000000) t.next("move");
    
    // for (s_size_t i=0; i < num_buckets; i++) {
    //   bucket_offsets[i] = dest_offsets[i*num_blocks];
    //   //cout << i << ", " << bucket_offsets[i] << endl;
    // }
    // // last element is the total size n
    // bucket_offsets[num_buckets] = n;

    //t.next("transpose");
    //free_array(B);
    if (n > 1000000000) {
      //for (size_t i=0; i < 10; i++) cout << bucket_offsets[i] << endl;
    }
    return bucket_offsets;
  }

  // Parallel version
  template <typename InS, typename KeyS>
  sequence<size_t> count_sort(InS const &In,
			      slice_t<typename InS::value_type*> Out,
			      KeyS const &Keys,
			      size_t num_buckets) {
    size_t n = In.size();
    size_t max32 = ((size_t) 1) << 32;
    if (n < max32 && num_buckets < max32)
      // use 4-byte counters when larger ones not needed
      return count_sort_<uint32_t>(In, Out, Keys, num_buckets);
    return count_sort_<size_t>(In, Out, Keys, num_buckets);
  }
}
