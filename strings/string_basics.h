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
#include <string>
#include <cstring>
//#include <charconv> -- not widely available yet
#include "../sequence.h"

#include <sys/mman.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace pbbs {

  // Reads a character sequence from a file :
  //    if end is zero or larger than file, then returns full file
  //    if start past end of file then returns an empty string.
  inline sequence<char> char_seq_from_file(std::string filename,
					   size_t start=0, size_t end=0);

  // Writes a character sequence to a file, returns 0 if successful
  template <class CharSeq>
  int char_seq_to_file(CharSeq const &S, std::string filename);

  // Writes a character sequence to a stream
  template <class CharSeq>
  void char_seq_to_stream(CharSeq const &S, std::ostream& os);

  // Returns a sequence of sequences of characters, one per token.
  // The tokens are the longest contiguous subsequences of non space characters.
  // where spaces are define by the unary predicate is_space.
  //template <class Seq, class UnaryPred>
  //sequence<sequence<char>> tokens(Seq const &S, UnaryPred const &is_space);

  // similar but the spaces are given as a string
  //  sequence<sequence<char>> tokens(Seq const &S, std::string const &spaces);

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
    bytes[n] = 0;
    return sequence<char>(bytes,n);
  }

  // Reads a file using mmap, returning it as a range
  inline range<char*> char_range_from_file(std::string filename) {
    // old fashioned c to deal with mmap
    struct stat sb;
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) { perror("open"); exit(-1);  }
    if (fstat(fd, &sb) == -1) { perror("fstat"); exit(-1); }
    if (!S_ISREG (sb.st_mode)) { perror("not a file\n");  exit(-1); }
    
    char *p = static_cast<char*>(mmap(0, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (p == MAP_FAILED) { close(fd); perror("mmap"); exit(-1); }
    if (close(fd) == -1) { perror("close"); exit(-1); }
    return range<char*>(p, p + sb.st_size);
  }

  template <class CharSeq>
  void char_seq_to_file_map(CharSeq const &S, std::string filename) {
    size_t n = S.size();
    timer t;
    
    mode_t mode = 600;
    int fd = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, mode);
    if (fd == -1) {perror("open"); exit(-1);}

    int tr = ftruncate(fd, n + 1);
    if (tr == -1) {close(fd); perror("truncate"); exit(-1);}
    t.next("truncate");
    
    char *p = static_cast<char*>(mmap(0, n, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (p == MAP_FAILED) { close(fd); perror("mmap"); exit(-1); }
    t.next("mmap");

    memcpy(p, S.begin(), n);
    t.next("pfor");
    
    if (munmap(p, n) == -1)
      { close(fd);  perror("munmap"); exit(-1);}
    t.next("unmap");
    
    close(fd);
    t.next("close");
  }
  
  template <class CharSeq>
  void char_seq_to_stream(CharSeq const &S, std::ostream& os) {
    os.write(S.begin(), S.size());
  }

  template <class CharSeq>
  int char_seq_to_file(CharSeq const &S, std::string filename) {
    std::ofstream file_stream (filename, std::ios::out | std::ios::binary);
    if (!file_stream.is_open()) {
      std::cout << "Unable to open file for writing: " << filename << std::endl;
      return 1;
    }
    char_seq_to_stream(S, file_stream);
    file_stream.close();
    return 0;
  }

  template <class Seq, class UnaryPred>
  sequence<sequence<char>> tokensa(Seq const &S, UnaryPred const &is_space) {
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
  sequence<sequence<char>>
  tokens(Seq const &S, UnaryPred const &is_space) {
    size_t n = S.size();
    char* s = S.begin();
    sequence<bool> Flags(n+1);
    if (n == 0) return sequence<sequence<char>>();

    parallel_for(1, n, [&] (long i) {
	Flags[i] = is_space(s[i-1]) != is_space(s[i]);
      }, 10000);
      
    Flags[0] = !is_space(s[0]);
    Flags[n] = !is_space(s[n-1]);

    sequence<long> Locations = pbbs::pack_index<long>(Flags);
  
    return sequence<sequence<char>>(Locations.size()/2, [&] (size_t i) {
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

  template <class Seq>
  sequence<sequence<char>> split(Seq const &S, std::string const &spaces) {
    auto is_space = [&] (char a) {
      for (int i = 0; i < spaces.size(); i++)
	if (a == spaces[i]) return true;
      return false;
    };
    return split(S, is_space);
  }

  // unlike atol, need not be null terminated
  template <class Seq>
  long char_seq_to_l(Seq str) {
    size_t i = 0;
    auto read_digits = [&] () {
      long r = 0;
      while (i < str.size() && std::isdigit(str[i]))
	r = r*10 + (str[i++] - 48);
      return r;
    };
    if (str[i] == '-') {i++; return -read_digits();}
    else return read_digits();
  }

  // ********************************
  // Printing to a character sequence
  // ********************************
  
  sequence<char> to_char_seq(bool v) {
    return singleton(v ? '1' : '0');
  }

  sequence<char> to_char_seq(long v) {
    int max_len = 21;
    char s[max_len+1];
    int l = snprintf(s, max_len, "%ld", v);
    return sequence<char>(range<char*>(s, s + std::min(max_len-1, l)));
  }

  sequence<char> to_char_seq(int v) {
    return to_char_seq((long) v);};

  sequence<char> to_char_seq(unsigned long v) {
    int max_len = 21;
    char s[max_len+1];
    int l = snprintf(s, max_len, "%lu", v);
    return sequence<char>(range<char*>(s, s + std::min(max_len-1, l)));
  }

  sequence<char> to_char_seq(unsigned int v) {
    return to_char_seq((unsigned long) v);};

  sequence<char> to_char_seq(double v) {
    int max_len = 20;
    char s[max_len+1];
    int l = snprintf(s, max_len, "%.11le", v);
    return sequence<char>(range<char*>(s, s + std::min(max_len-1, l)));
  }

  sequence<char> to_char_seq(float v) {
    return to_char_seq((double) v);};

  sequence<char> to_char_seq(std::string const &s) {
    return map(s, [&] (char c) {return c;});
  }

  sequence<char> to_char_seq(const char* s) {
    return tabulate(strlen(s), [&] (size_t i) {return s[i];});
  }
  
  template <class A, class B>
  sequence<char> to_char_seq(std::pair<A,B> const &P) {
    sequence<sequence<char>> s = {
      singleton('('), to_char_seq(P.first),
      to_char_seq(", "),
      to_char_seq(P.second), singleton(')')};
  return flatten(s);
  }
    
  template <class Seq>
  sequence<char> to_char_seq(Seq const &A) {
    auto n = A.size();
    if (n==0) return to_char_seq("[]");
    auto separator = to_char_seq(", ");
    return flatten(tabulate(2 * n + 1, [&] (size_t i) {
	  if (i == 0) return singleton('[');
	  if (i == 2*n) return singleton(']');
	  if (i & 1) return to_char_seq(A[i/2]);
	  return separator;
	}));
  }

  sequence<char> to_char_seq(sequence<uchar> const &s) {
    return map(s, [&] (uchar c) {return (char) c;});
  }

  sequence<char> to_char_seq(sequence<char> s) { return s; }
  
  // std::to_chars not widely available yet. sigh!!
  // template<typename T, 
  // 	   typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
  // sequence<char> to_char_seq(T v) {
  //   char a[20];
  //   auto r = std::to_chars(a, a+20, v);
  //   return sequence<char>(range<char*>(a, r.ptr));
  // }
   
}

