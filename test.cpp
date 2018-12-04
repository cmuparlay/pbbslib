#include "scheduler.h"
#include "get_time.h"
#include "parse_command_line.h"
#include "utilities.h"

using namespace std;

fork_join_scheduler fj;
//fork_join_scheduler fj2;

long fib(long i) {
  if (i <= 1) return 1;
  else if (i < 18) return fib(i-1) + fib(i-2);
  long l,r;
  fj.pardo([&] () { l = fib(i-1);},
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
  fj.run(job,p);


  auto job2 = [&] () {
    long* a = new long[m];
    auto ident = [&] (int i) {a[i] = i;};
    fj.parfor(0,m,ident,2000);
    timer t2;
    for (int i=0; i < 100; i++) {
      fj.parfor(0,m,ident,2000);
    }
    t2.next("tabulate");
  };
  fj.run(job2,p);

  auto spin = [&] (int i) {
    for (volatile int j=0; j < 1000; j++);
  };

  auto job3 = [&] () {
    fj.parfor(0,m/200,spin,20);
    timer t2;
    for (int i=0; i < 100; i++) {
      fj.parfor(0,m/200,spin,20);
    }
    t2.next("map spin");
  };
  fj.run(job3,p);

  timer t2;
#pragma omp parallel for
  for (int i=0; i < 100; i++) {
#pragma omp parallel for schedule(static,20)
    for (size_t k=0 ; k < m/200; k++) spin(k);
  }
  t2.next("map spin omp");
}
  
  

