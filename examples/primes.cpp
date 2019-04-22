// Primes.
// Generates the first n primes and either writes them to a specified
// output file (-o <outfile> or if no file is given, reports the
// number of primes up to n.

#include "sequence.h"
#include "get_time.h"
#include "strings/string_basics.h"
#include "parse_command_line.h"

using namespace pbbs;

// Recursively finds primes less than sqrt(n), then sieves out
// all their multiples, returning the primes less than n.
// Work is O(n log log n), Span is O(log n).
template <class Int>
sequence<Int> prime_sieve(Int n) {
  if (n < 2) return sequence<Int>();
  else {
    Int sqrt = std::sqrt(n);
    auto primes_sqrt = prime_sieve(sqrt);
    sequence<bool> flags(n+1, true);  // flags to mark the primes
    flags[0] = flags[1] = false;  // 0 and 1 are not prime
    parallel_for(0, primes_sqrt.size(), [&] (size_t i) {
	Int prime = primes_sqrt[i];
	parallel_for(2, n/prime + 1, [&] (size_t j) {
	    flags[prime * j] = false;}, 1000);
      }, 1);
    return pack_index<Int>(flags);  // indices of the primes
  }
}

int main (int argc, char *argv[]) {
  commandLine P(argc, argv, "[-r <rounds>] [-o <outfile>] n");
  int rounds = P.getOptionIntValue("-r", 1);
  size_t n = std::stol(P.getArgument(0));
  std::string outfile = P.getOptionValue("-o", "");
  timer t("primes", true);

  sequence<long> primes;
  for (int i=0; i < rounds; i++) {
    primes = prime_sieve((long) n);
    t.next("calculate primes");
  }
  
  if (outfile.size() > 0) {
    auto out_str = to_char_seq(primes);
    t.next("generate output string");

    char_seq_to_file(out_str, outfile);
    t.next("write file");
  } else cout << "number of primes = " << primes.size() << endl;



}

