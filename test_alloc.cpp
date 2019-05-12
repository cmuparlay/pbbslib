#include "utilities.h"
#include "sequence.h"
#include "get_time.h"
#include "list_allocator.h"
#include <vector>
using namespace pbbs;
using namespace std;


template <typename T, typename Allocator=pbbs::allocator<T>>
  struct my_vect {
    size_t n;
    Allocator allocator;
    
    my_vect(size_t n, const Allocator &allocator = Allocator()) : n(n), allocator(allocator) {}
  };

int main (int argc, char *argv[]) {
  //small_allocator pool;
  size_t n = 100000000;
  int rounds = 4;
  timer t;

  my_vect<double> a(10);

  cout << "hello: " << sizeof(a) << endl;


  
  void *x = aligned_alloc(256, ((size_t) 1) << 35);
  parallel_for (0, (((size_t) 1) << 25), [&] (size_t i) {
       ((char*) x)[i * (1 << 10)] = 1;}, 1000);
  free(x);
  t.next("init");

  // size_t bits = 30; // 22
  // size_t num_blocks = 8; //2000;
  // sequence<void*> a(num_blocks);
  // parallel_for(0, num_blocks, [&] (size_t j) {
  //     void * x = aligned_alloc(256, 1 << bits);
  //     parallel_for (0, (1 << (bits - 18)), [&] (size_t i) {
  //      ((char*) x)[i * (1 << 18)] = 1;}, 10000);
  //     return x;
  //   }, 1);
  // parallel_for(0, num_blocks, [&] (size_t j) {
  //     free(a[j]);});
  // t.next("init");

  {
    //mem_pool mp;
    for (int i=0; i < rounds; i++) {
      sequence<char *> b(n, [&] (size_t i) {
	  char* foo = (char*) my_alloc(48);
	  foo[0] = 'a';
	  foo[1] = 0;
	  return foo;
	});
      t.next("allocate");
      parallel_for (0,b.size(), [&] (size_t i) {
	  my_free(b[i]);
	});
      t.next("free");
      //default_allocator.print_stats();
    }
  }
  //t.next("delete allocator");
  cout << endl;

  {
    std::vector<size_t> sizes = {16, 32, 64, 128};
    pool_allocator sa(sizes);
    t.next("initialize");
    for (int i=0; i < rounds; i++) {
      sequence<char *> b(n, [&] (size_t i) {
	  char* foo = (char*) sa.allocate(64);
	  foo[0] = 'a';
	  foo[1] = 0;
	  return foo;
	});
      t.next("small allocate");
      parallel_for (0,b.size(), [&] (size_t i) {
	  sa.deallocate(b[i], 64);
	});
      t.next("small free");
    }
  }
  t.next("delete allocator");
  cout << endl;

  {
    block_allocator ba(64);
    t.next("initialize");
    for (int j=0; j < rounds; j++) {
      sequence<char *> b(n, [&] (size_t i) {
	  int l = log2_up(i);
	  char* foo = (char*) ba.alloc();
	  foo[0] = l;
	  foo[1] = 0;
	  return foo;
	});
      t.next("block allocate");
      parallel_for (0,b.size(), [&] (size_t i) {
	  ba.free(b[i]);
	});
      t.next("block free");
    }
  }
  t.next("delete allocator");
  cout << endl;

  {
    using T = std::array<long,8>;
    using la = list_allocator<T>;
    la::init();
    t.next("initialize");
    for (int j=0; j < rounds; j++) {
      sequence<char *> b(n, [&] (size_t i) {
	  char* foo = (char*) la::alloc();
	  foo[0] = 'a';
	  foo[1] = 0;
	  return foo;
	});
      t.next("list allocate");
      parallel_for (0,b.size(), [&] (size_t i) {
	  la::free((T*) b[i]);
	});
      t.next("list free");
    }
    //la::finish();
  }
  //t.next("clear allocator");
  cout << endl;
  
    for (int j=0; j < rounds; j++) {
    sequence<char *> b(n, [&] (size_t i) {
	char* foo = (char*) malloc(48);
	foo[0] = 'a';
	foo[1] = 0;
	return foo;
      });
    t.next("malloc");
    parallel_for (0,b.size(), [&] (size_t i) {
	free(b[i]);
      });
    t.next("list free");
  }
  cout << endl;

}
