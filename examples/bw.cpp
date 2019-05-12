#include "sequence.h"
#include "counting_sort.h"
#include "get_time.h"
#include "strings/string_basics.h"
#include "strings/suffix_array.h"
#include "strings/lcp.h"
#include "parse_command_line.h"
#include "random.h"
#include "stlalgs.h"

using namespace pbbs;

template <class Int>
std::pair<sequence<uchar>, size_t> bw_transform(sequence<uchar> const &s) {
  size_t n = s.size();

  // sort on suffixes to order previous characters
  // skip first character since there is no previous character.
  auto sa = suffix_array<Int>(s.slice(1,n));

  // get previous char for each suffix (sorted)
  // last char is s goes first since it has an empty suffix (lowest rank)
  auto bwt = tabulate(n, [&] (size_t i) -> uchar {
      return (i == 0) ? s[n-1] : s[sa[i-1]];});

  // location in bwt of first character in s
  size_t loc = find(sa,0)+1;
  return std::make_pair(bwt, loc);
}

// separates the head pointer from the rest of the string
std::pair<size_t, range<uchar*>> split_head(sequence<uchar> const &s) {
  size_t l = find(s, ':');
  size_t head = atol((char*) s.begin());
  return std::make_pair(head, s.slice(l+1, s.size()));
}

template <class Int>
sequence<uchar> bw_transform_reverse_(sequence<uchar> const &s) {
  timer t("trans", false);

  Int head;
  range<uchar*> ss;
  std::tie(head, ss) = split_head(s);
  Int n = ss.size();

  // Set head location to 0 temporarily so when sorted it ends up
  // first this is the right position since the last character in the
  // original string is the first when sorted by suffix, and head is
  // its "suffix" with wraparound
  uchar a = ss[head];
  ss[head] = 0;

  // sort character, returning original locations in sorted order
  auto sorted = count_sort(iota<Int>(n), ss, 256);
  ss[head] = a;
  t.next("count sort");

  Int block_bits = 10;
  Int block_size = 1 << block_bits;
  pbbs::random r(0);
  auto links = sorted.first;
  sequence<bool> head_flags(n, false);

  // pick a set of random locations as heads (each with prob 1/block_size).
  // links that point to a head are set to their original position + n
  parallel_for(0, n, [&] (Int i) {
      if ((r.ith_rand(i) & (block_size-1)) == 0) {
	head_flags[links[i]] = true;
	links[i] += n;
      } 
    });
  head_flags[head] = true;  // the overall head is made to be a head
  links[0] += n;   // this points to the original head
  t.next("set next");

  // indices of heads;
  auto heads = pack_index<Int>(head_flags);
  t.next("pack index");

  // map over each head and follow the list until reaching the next head
  // as following the list, add characters to a buffer
  // at the end return the buffer trimmed to fit the substring exactly
  // also return pointer to the next head
  auto blocks = map(heads, [&] (Int my_head) {
      // very unlikely to take more than this much space, throws an exception if it does
      Int buffer_len = block_size * 30;
      sequence<uchar> buffer(buffer_len);
      Int i = 0;
      Int pos = my_head;
      do {
	buffer[i++] = ss[pos];
	if (i == buffer_len)
	  throw std::runtime_error("ran out of buffer space in bw decode");
	pos = links[pos];
      } while (pos < n);
      sequence<uchar> trimmed(buffer.slice(0,i));
      return std::make_pair(std::move(trimmed), pos - n);
    });
  t.next("follow pointers");

  // location in heads for each head in s
  sequence<Int> location_in_heads(n);
  Int m = heads.size();
  parallel_for(0, m, [&] (Int i) {
      location_in_heads[heads[i]] = i; });
  t.next("enumerate");

  // start at first block and follow next pointers
  // putting each into ordered_blocks
  Int pos = head;
  sequence<sequence<uchar>> ordered_blocks(m);
  for (Int i=0; i < m; i++) {
    Int j = location_in_heads[pos];
    ordered_blocks[i] = std::move(blocks[j].first);
    pos = blocks[j].second;
  }
  t.next("seq");

  // flatten ordered blocks into final string
  auto res = flatten(ordered_blocks);
  t.next("flatten");
  return res;
}

sequence<uchar> bw_transform_reverse(sequence<uchar> const &s) {
  if (s.size() >= (((long) 1) << 31))
    return bw_transform_reverse_<unsigned long>(s);
  else
    return bw_transform_reverse_<unsigned int>(s);
}
  
int main (int argc, char *argv[]) {
  commandLine P(argc, argv, "[-r <rounds>] [-o] infile");
  int rounds = P.getOptionIntValue("-r", 1);
  bool output = P.getOption("-o");
  bool detrans = P.getOption("-d");
  char* filename = P.getArgument(0);
  timer t("bw", !output);

  auto istr = pbbs::char_range_from_file(filename);
  auto str = map(istr, [&] (char c) { return (uchar) c;});
  t.next("read file");

  size_t loc=0;
  sequence<uchar> out;
    
  for (int i=0; i < rounds; i++) {
    if (detrans)
      out = bw_transform_reverse(str);
    else 
      std::tie(out, loc) = bw_transform<uint>(str);
    t.next("calculate bw transform");
  }
  
  if (output) {
    auto ostr = map(out, [&] (uchar c) {return (char) c;});
    if (!detrans)
      cout << loc << ':';
    cout << ostr;
  }
}
