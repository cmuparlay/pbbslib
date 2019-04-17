#include "sequence.h"
#include "get_time.h"
#include "strings/string_basics.h"
#include "parse_command_line.h"
#include "group_by.h"

using namespace pbbs;

template <class Int>
sequence<Int> prime_sieve(Int n) {
  if (n < 2) return sequence<Int>();
  else {
    Int sqrt = std::sqrt(n);
    auto primes_sqrt = prime_sieve(sqrt);
    sequence<bool> flags(n+1, true);
    flags[0] = flags[1] = false;
    parallel_for(0, sqrt+1, [&] (size_t i) {flags[i] = false;});
    parallel_for(0, primes_sqrt.size(), [&] (size_t i) {
	Int prime = primes_sqrt[i];
	flags[prime] = true;
	parallel_for(sqrt/prime + 1, n/prime + 1, [&] (size_t j) {
	    flags[prime*j] = false;}, 1000);
      }, 1000);
    size_t c = reduce(map(primes_sqrt, [&] (size_t prime) -> size_t {
	  return n/prime - sqrt/prime;}), addm<size_t>());
    cout << c << endl;
    return pack_index<Int>(flags);
  }
}

int main (int argc, char *argv[]) {
  commandLine P(argc, argv, "[-r <rounds>] [-o <outfile>] n");
  int rounds = P.getOptionIntValue("-r", 1);
  size_t n = std::stoi(P.getArgument(0));
  std::string outfile = P.getOptionValue("-o", "");
  timer t("primes", true);

  sequence<int> primes;
  for (int i=0; i < rounds; i++) {
    primes = prime_sieve((int) n);
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

