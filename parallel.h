#pragma once

// cilkplus
#if defined(CILK)
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <sstream>
#define parallel_for cilk_for
#define PAR_GRANULARITY 5000

static int num_workers() {return __cilkrts_get_nworkers();}
static int worker_id() {return __cilkrts_get_worker_number();}
void set_num_workers(int n) {
  __cilkrts_end_cilk();
  std::stringstream ss; ss << n;
  if (0 != __cilkrts_set_param("nworkers", ss.str().c_str())) {
    std::cerr << "failed to set worker count!" << std::endl;
    std::abort();
  }
}

template <typename Lf, typename Rf>
inline void par_do_(Lf left, Rf right) {
    cilk_spawn right();
    left();
    cilk_sync;
}

template <typename Lf, typename Mf, typename Rf >
inline void par_do3_(Lf left, Mf mid, Rf right) {
    cilk_spawn mid();
    cilk_spawn right();
    left();
    cilk_sync;
}

// openmp
#elif defined(OPENMP)
#include <omp.h>
#define parallel_for _Pragma("omp parallel for") for
#define PAR_GRANULARITY 200000

static int num_workers() { return omp_get_max_threads(); }
static int worker_id() { return omp_get_thread_num(); }
void set_num_workers(int n) { omp_set_num_threads(n); }

template <typename Lf, typename Rf>
static void par_do_(Lf left, Rf right) {
#pragma omp task
    left();
#pragma omp task
    right();
#pragma omp taskwait
}

template <typename Lf, typename Mf, typename Rf>
static void par_do3_(Lf left, Mf mid, Rf right) {
#pragma omp task
    left();
#pragma omp task
    mid();
#pragma omp task
    right();
#pragma omp taskwait
}

// c++
#else
#define parallel_for for

static int num_workers() { return 1;}
static int worker_id() { return 0;}
void set_num_workers(int n) { ; }
#define PAR_GRANULARITY 1000

template <typename Lf, typename Rf>
static void par_do_(Lf left, Rf right) {
  left(); right();
}

template <typename Lf, typename Mf, typename Rf>
static void par_do3_(Lf left, Mf mid, Rf right) {
  left(); mid(); right();
}
#endif
