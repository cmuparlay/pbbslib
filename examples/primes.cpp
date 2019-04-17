#include "sequence.h"
#include "get_time.h"
#include "strings/string_basics.h"
#include "parse_command_line.h"

using namespace pbbs;

template <class Int>
sequence<Int> prime_sieve(Int n) {
  if (n < 2) return sequence<Int>();
  else {
    Int sqrt = std::sqrt(n);
    auto primes_sqrt = prime_sieve(sqrt);
    sequence<bool> flags(n+1, true);
    flags[0] = flags[1] = false;
    parallel_for(0, primes_sqrt.size(), [&] (size_t i) {
	Int prime = primes_sqrt[i];
	parallel_for(2, n/prime + 1, [&] (size_t j) {
	    flags[prime*j] = false;}, 1000);
      }, 1);
    return pack_index<Int>(flags);
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
  
  cout << primes.size() << endl;

  if (outfile.size() > 0) {
    auto out_str = to_char_seq(primes);
    t.next("generate output string");

    char_seq_to_file(out_str, outfile);
    t.next("write file");
  }

}

