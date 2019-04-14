// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2011-2019 Guy Blelloch and the PBBS team
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

#include <iostream>
#include <fstream>
#include "../sequence.h"

namespace pbbs {

  // Reads a character sequence from a file :
  //    if end is zero or larger than file, then returns full file
  //    if start past end of file then returns an empty string.
  inline sequence<char> char_seq_from_file(std::string filename,
					   size_t start=0, size_t end=0);

  // Writes a character sequence to a file, returns 0 if successful
  template <class CharSeq>
  int char_seq_to_file(CharSeq const &S, std::string fileName);

  // Returns a sequence of sequences of characters, one per token.
  // The tokens are the longest contiguous subsequences of non space characters.
  // where spaces are define by the unary predicate is_space.
  template <class Seq, class UnaryPred>
  sequence<sequence<char>> tokens(Seq const &S, UnaryPred const &is_space);

  // A more primitive version of tokens.
  // Zeros out all spaces, and returns a pointer to the start of each token.
  // Can be used with c style char* functions on each token since they will be null
  // terminated.
  template <class Seq, class UnaryPred>
  sequence<char*> tokenize(Seq &S, UnaryPred const &is_space);

  // Returns a sequence of character ranges, one per partition.
  // The StartFlags sequence specifies the start of each partition.
  // The two arguments must be of the same length.
  // Location 0 is always a start.
  template <class Seq, class BoolSeq>
  auto partition_at(Seq const &S, BoolSeq const &StartFlags)
    -> sequence<range<typename Seq::value_type *>>;

  // ********************************
  // Code Follows
  // ********************************
  inline sequence<char> char_seq_from_file(std::string filename,
					   size_t start, size_t end) {
    std::ifstream file (filename, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      std::cout << "Unable to open file: " << filename << std::endl;
      exit(1);
    }
    size_t length = file.tellg();
    start = std::min(start,length);
    if (end == 0) end = length;
    else end = std::min(end,length);
    size_t n = end - start;
    file.seekg (start, std::ios::beg);
    char* bytes = new_array<char>(n+1);
    file.read (bytes,n);
    file.close();
    return sequence<char>(bytes,n);
  }

  template <class CharSeq>
  int char_seq_to_file(CharSeq const &S, std::string fileName) {
    size_t n = S.size();
    std::ofstream file (fileName, std::ios::out | std::ios::binary);
    if (!file.is_open()) {
      std::cout << "Unable to open file for writing: " << fileName << std::endl;
      return 1;
    }
    file.write(S.begin(), n);
    file.close();
    return 0;
  }

  template <class Seq, class UnaryPred>
  sequence<sequence<char>> tokens(Seq const &S, UnaryPred const &is_space) {
    size_t n = S.size();
    if (n==0) return sequence<sequence<char>>();
    sequence<bool> Flags(n+1);
    parallel_for(1, n, [&] (long i) {
	Flags[i] = is_space(S[i-1]) != is_space(S[i]);
      }, 10000);
    Flags[0] = !is_space(S[0]);
    Flags[n] = !is_space(S[n-1]);

    sequence<long> Locations = pbbs::pack_index<long>(Flags);
  
    return tabulate(Locations.size()/2, [&] (size_t i) {
	return sequence<char>(S.slice(Locations[2*i], Locations[2*i+1]));});
  }

  template <class Seq, class UnaryPred>
  sequence<char*> tokenize(Seq  &S, UnaryPred const &is_space) {
    size_t n = S.size();

    // clear spaces
    parallel_for (0, n, [&] (size_t i) {
	if (is_space(S[i])) S[i] = 0;}, 10000);
    S[n] = 0;

    auto StartFlags = delayed_seq<bool>(n, [&] (long i) {
	return (i==0) ? S[i] : S[i] && !S[i-1];});

    auto Pointers = delayed_seq<char*>(n, [&] (long i) {
	return S.begin() + i;});

    sequence<char*> r = pbbs::pack(Pointers, StartFlags);

    return r;
  }

  template <class Seq, class BoolSeq>
  auto partition_at(Seq const &S, BoolSeq const &StartFlags)
    -> sequence<range<typename Seq::value_type *>>
  {
    using T = typename Seq::value_type;
    size_t n = S.size();
    if (StartFlags.size() != n)
      std::cout << "Unequal sizes in pbbs::partition_at" << std::endl;
    auto sf = delayed_seq<bool>(n, [&] (size_t i) {
	return (i==0) || StartFlags[i];});

    sequence<long> Starts = pbbs::pack_index<long>(sf);
    return sequence<range<T*>>(Starts.size(), [&] (size_t i) {
	long end = (i==Starts.size()-1) ? n : Starts[i+1];
	return range<T*>(S.slice(Starts[i],end));});			    
  }

  template <class Seq, class UnaryPred>
  sequence<sequence<char>> split(Seq const &S, UnaryPred const &is_space) {
    size_t n = S.size();

    auto X = sequence<bool>(n, [&] (size_t i) {
	return is_space(S[i]);});
    sequence<long> Locations = pbbs::pack_index<long>(X);
    size_t m = Locations.size();
  
    return tabulate(m + 1, [&] (size_t i) -> sequence<char> {
	size_t start = (i==0) ? 0 : Locations[i-1] + 1;
	size_t end = (i==m) ? n : Locations[i];
	return sequence<char>(S.slice(start, end));});
  }

  inline int to_chars_len(long a) { return 21;}
  inline void to_chars_(char* s, long a) { sprintf(s,"%ld",a);}

  inline int to_chars_len(unsigned long a) { return 21;}
  inline void to_chars_(char* s, unsigned long a) { sprintf(s,"%lu",a);}

  inline uint to_chars_len(uint a) { return 12;}
  inline void to_chars_(char* s, uint a) { sprintf(s,"%u",a);}

  inline int to_chars_len(int a) { return 12;}
  inline void to_chars_(char* s, int a) { sprintf(s,"%d",a);}

  inline int to_chars_len(double a) { return 18;}
  inline void to_chars_(char* s, double a) { sprintf(s,"%.11le", a);}

  inline int to_chars_len(char* a) { return strlen(a)+1;}
  inline void to_chars_(char* s, char* a) { sprintf(s,"%s",a);}

  template <class A, class B>
  inline int to_chars_len(std::pair<A,B> a) { 
    return to_chars_len(a.first) + to_chars_len(a.second) + 1;
  }

  template <class A, class B>
  inline void to_chars_(char* s, std::pair<A,B> a) { 
    int l = to_chars_len(a.first);
    to_chars_(s, a.first); s[l] = ' '; to_chars_(s+l+1, a.second);
  }

  template <class Seq>
  sequence<char> to_char_seq(Seq const &A, char separator=' ') {
    size_t n = A.size();
    sequence<long> L(n, [&] (size_t i) -> size_t {
	return to_chars_len(A[i])+1;});
    size_t m = pbbs::scan_inplace(L.slice(), addm<size_t>());
    
    sequence<char> B(m+1, (char) 0);
    char* Bs = B.begin();
    
    parallel_for(0, n-1, [&] (long i) {
	to_chars_(Bs + L[i], A[i]);
	Bs[L[i+1] - 1] = separator;
      });
    to_chars_(Bs + L[n-1], A[n-1]);
    auto r = pbbs::filter(B, [&] (char c) {return c != 0;});
    return r;
  }
  
}

