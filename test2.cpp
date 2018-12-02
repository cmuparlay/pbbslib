#include "utilities.h"
#include "get_time.h"
#include "parse_command_line.h"
#include <iostream>

using namespace std;

long fib(long i) {
  if (i <= 1) return 1;
  else if (i < 15) return fib(i-1) + fib(i-2);
  long l,r;
  par_do(true,
	 [&] () { l = fib(i-1);},
	 [&] () { r = fib(i-2);});
  return l + r;
}

int main (int argc, char *argv[]) {
  commandLine P(argc, argv, "[-n <size>] [-p <threads>]");
  size_t n = P.getOptionLongValue("-n", 40);
  size_t m = P.getOptionLongValue("-m", 100000000);
  size_t p = P.getOptionLongValue("-p", 0);

  auto job = [&] () {
    timer t;
    long r = fib(n);
    t.next("fib");
    cout << "result: " << r << endl;
  };
  job();


  auto job2 = [&] () {
    long* a = new long[m];
    auto ident = [&] (int i) {a[i] = i;};
    par_for(0,m,2000,ident);
    timer t2;
    for (int i=0; i < 10; i++) {
      par_for(0,m,2000,ident);
    }
    t2.next("tabulate");
  };
  job2();
}
  
  

