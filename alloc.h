#pragma once

#ifdef USEMALLOC
inline void* my_alloc(size_t i) {return malloc(i);}
inline void my_free(void* p) {free(p);}
void allocator_clear() {}

#else

#include <atomic>
#include <vector>
#include "utilities.h"
#include "concurrent_stack.h"
#include "utilities.h"
#include "block_allocator.h"
#include "memory_size.h"

// allocates local lists for each core using the block_allocator
// pools are powers of 2 from 2^{log_min_size} to 2^{log_max_size}
struct small_allocator {

  static const int total_list_size = (1 << 22);
  int num_buckets; 
  struct block_allocator *allocators;
  std::vector<int> sizes;

  ~small_allocator() {
    //for (int i=0; i < num_buckets; i++)
    //  ~block_allocator(allocators[i]));
    //free(allocators);
  }

  small_allocator() {}
  
  small_allocator(std::vector<int> const &sizes) : sizes(sizes) {
    num_buckets = sizes.size();
    allocators = (struct block_allocator*)
      malloc(num_buckets * sizeof(struct block_allocator));
    
    for (int i = 0; i < num_buckets; i++) {
      size_t bucket_size = sizes[i];
      size_t per_proc_list_size = total_list_size / bucket_size;
      size_t initial_additional_blocks = 0;
      new (static_cast<void*>(std::addressof(allocators[i]))) 
	block_allocator(bucket_size, initial_additional_blocks,
			per_proc_list_size);
    }
  }

  void* alloc(int n) {
    if (n < 0) abort();
    int bucket = 0;
    while (n > sizes[bucket]) {
      bucket++;
      if (bucket == (int) sizes.size()) abort();
    }
    return allocators[bucket].alloc();
  }

  void free(void* ptr, int n) {
    if (n < 0) abort();
    int bucket = 0;
    while (n > sizes[bucket]) {
      bucket++;
      if (bucket == (int) sizes.size()) abort();
    }
    allocators[bucket].free(ptr);
  }

  void print_stats() {
    size_t total_a = 0;
    size_t total_u = 0;
    for (int i = 0; i < num_buckets; i++) {
      size_t bucket_size = sizes[i];
      size_t allocated = allocators[i].num_allocated_blocks();
      size_t used = allocators[i].num_used_blocks();
      total_a += allocated * bucket_size;
      total_u += used * bucket_size;
      cout << "size = " << bucket_size << ", allocated = " << allocated
	   << ", used = " << used << endl;
    }
    cout << "Total bytes allocated = " << total_a << endl;
    cout << "Total bytes used = " << total_u << endl;
  }
    
};

// for small allocations uses the small_allocator above
// otherwise gets directly from a lock-free shared concurrent stack 
struct mem_pool {
  concurrent_stack<void*>* buckets;
  static constexpr int allocator_header_size = 16;
  static constexpr int large_align = 64;
  static constexpr int log_base = 18;
  static constexpr int log_min_size = 4;
  static constexpr int num_buckets = 22;
  std::atomic<long> allocated{0};
  std::atomic<long> used{0};
  size_t mem_size{getMemorySize()};
  struct small_allocator small_alloc;

  mem_pool() {
    buckets = new concurrent_stack<void*>[num_buckets];

    std::vector<int> sizes;
    for (int i = log_min_size; i < log_base; i++)
      sizes.push_back(1 << i);
    small_alloc = small_allocator(sizes);
  };

  void* add_header(void* a) {
    return (void*) ((char*) a + allocator_header_size);}

  void* add_large_header(void* a) {
    return (void*) ((char*) a + large_align);}

  void* sub_header(void* a) {
    return (void*) ((char*) a - allocator_header_size);}

  void* sub_large_header(void* a) {
    return (void*) ((char*) a - large_align);}

  void* alloc(size_t s) {
    size_t padded_size = s + allocator_header_size;
    int log_size = pbbs::log2_up(padded_size);
    if (log_size < log_base) {
      void* a = small_alloc.alloc(1 << log_size);
      *((int*) a) = log_size;
      return add_header(a);
    }
    int bucket = log_size - log_base;
    maybe<void*> r = buckets[bucket].pop();
    size_t n = ((size_t) 1) << log_size;
    used += n;
    if (r) {
      return add_large_header(*r);
    }
    else {
      void* a = (void*) aligned_alloc(large_align, n);
      void* r = add_large_header(a);
      allocated += n;
      if (a == NULL) std::cout << "alloc failed" << std::endl;
      // a hack to make sure pages are touched in parallel
      // not the right choice if you want processor local allocations
      size_t stride = (1 << 21); // 2 Mbytes in a huge page
      auto touch_f = [&] (size_t i) {((bool*) a)[i*stride] = 0;};
      parallel_for(0, n/stride, touch_f, 1);
      *((int*) sub_header(r)) = log_size;
      return r;
    }
  }

  void afree(void* a) {
    int log_size = *((int*) sub_header(a));
    if (log_size < log_base)
      small_alloc.free(sub_header(a), (1 << log_size));
    else if (log_size >= log_base + num_buckets) {
      std::cout << "corrupted header in free" << std::endl;
      abort();
    } else {
      int bucket = log_size - log_base;
      void* b = sub_large_header(a);
      size_t n = ((size_t) 1) << log_size;
      used -= n;
      if (n > mem_size/64) { 
	free(b);
	allocated -= n;
      } else {
	buckets[bucket].push(b);
      }
    }
  }

  void clear() {
    for (size_t i = 0; i < num_buckets; i++) {
      size_t n = ((size_t) 1) << (i+log_base);
      maybe<void*> r = buckets[i].pop();
      while (r) {
	allocated -= n;
	free(*r);
	r = buckets[i].pop();
      }
    }
  }
};

static mem_pool my_mem_pool;

inline void* my_alloc(size_t i) {return my_mem_pool.alloc(i);}
inline void my_free(void* p) {my_mem_pool.afree(p);}

void allocator_clear() {
  my_mem_pool.clear();
}
#endif
