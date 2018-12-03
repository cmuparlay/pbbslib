#include "utilities.h"
#include "scheduler.h"
#include "get_time.h"
#include "parse_command_line.h"
#include <iostream>

using namespace std;

fork_join_scheduler fj;
fork_join_scheduler fj2;

long fib(long i) {
  if (i <= 1) return 1;
  else if (i < 24) return fib(i-1) + fib(i-2);
  long l,r;
  fj.pardo([&] () { l = fib(i-1);},
	   [&] () { r = fib(i-2);});
  return l + r;
}

template <typename F>
static void parfor(size_t start, size_t end, size_t granularity, F f) {
  if ((end - start) <= granularity)
    for (size_t i=start; i < end; i++) f(i);
  else {
    size_t n = end-start;
    size_t mid = ((((size_t) 1) << pbbs::log2_up(n) != n)
		  ? (end+start)/2
		  : start + (7*(n+1))/16);
    fj2.pardo([&] () {parfor(start, mid, granularity, f);},
	     [&] () {parfor(mid, end, granularity, f);});
  }
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
  fj.run(job,p);


  auto job2 = [&] () {
    long* a = new long[m];
    auto ident = [&] (int i) {a[i] = i;};
    parfor(0,m,2000,ident);
    timer t2;
    for (int i=0; i < 10000; i++) {
      parfor(0,m,2000,ident);
      t2.next("tabulate");
    }
  };
  fj2.run(job2,p);
}
  
  

