#include <omp.h>
#include <chrono>
#include <thread>

using namespace std;

// from Arora, Blumofe, Plaxton
template <typename Job>
struct Deque {
  using qidx = int;

  // use unit for atomic access
  union age_t {
    struct {
      int tag;
      qidx top;
    } pair;
    size_t unit;
  };

  struct alignas(64) padded_job { Job* job;  };
  
  age_t age;
  qidx bot;
  padded_job deq[100];

  inline bool cas(size_t* ptr, size_t oldv, size_t newv) {
    return __sync_bool_compare_and_swap(ptr, oldv, newv);
  }

  inline void write_fence() {
    std::atomic_thread_fence(std::memory_order_release);
  }

  inline void read_fence() {
    std::atomic_thread_fence(std::memory_order_acquire);
  }

  Deque() : bot(0) {
    age.pair.tag = 0;
    age.pair.top = 0;
  }
    
  void push_bottom(Job* job) {
    read_fence();
    qidx local_bot = bot; // atomic load
    deq[local_bot].job = job; // shared store
    local_bot += 1;
    bot = local_bot; // shared store
    write_fence(); 
  }
  
  Job* pop_top() {
    age_t old_age, new_age;
    //read_fence();
    old_age.unit = age.unit; // atomic load
    qidx local_bot = bot; // atomic load
    if (local_bot <= old_age.pair.top)
      return NULL;
    Job* job = deq[old_age.pair.top].job; // atomic load
    new_age.unit = old_age.unit;
    new_age.pair.top = new_age.pair.top + 1;
    if (cas(&(age.unit), old_age.unit, new_age.unit))  // cas
      return job;
    return NULL;
  }

  Job* pop_bottom() {
    age_t old_age, new_age;
    read_fence();
    qidx local_bot = bot; // atomic load
    if (local_bot == 0) 
      return NULL;
    local_bot = local_bot - 1;
    bot = local_bot; // shared store
    write_fence();
    read_fence();
    Job* job = deq[local_bot].job; // atomic load
    old_age.unit = age.unit; // atomic load
    if (local_bot > old_age.pair.top)
      return job;
    bot = 0; // shared store
    write_fence();  // not sure if this write_fence is needed
    new_age.pair.top = 0;
    new_age.pair.tag = old_age.pair.tag + 1;
    if (local_bot == old_age.pair.top) {
      if (cas(&(age.unit), old_age.unit, new_age.unit)) { // cas
	return job;
      }
    }
    age.unit = new_age.unit; // shared store
    write_fence();
    return NULL;
  }
};

template <typename Job>
struct scheduler {

public:
  scheduler() {
    num_deques = 2*num_workers();
    deques = new Deque<Job>[num_deques];
    attempts = new attempt[num_deques];
    finished_flag = 0;
  }

  ~scheduler() {
    delete[] deques;
    delete[] attempts;
  }

  void run(Job* job, int num_threads = 0) {
    deques[0].push_bottom(job);
    auto finished = [&] () {return finished_flag > 0;};
    if (num_threads > 0 && num_threads < 2 * num_workers())
      omp_set_num_threads(num_threads);
    #pragma omp parallel
    wait(finished);

    finished_flag = 0;
  }
    
  void spawn(Job* job) {
    int id = worker_id();
    deques[id].push_bottom(job);
  }

  template <typename F>
  void wait(F finished) {
    while (1) {
      Job* job = get_job(finished);
      if (!job) return;
      (*job)();
      //if (!finished()) std::this_thread::yield();
    }
  }

  void finish() {finished_flag = 1;}

  Job* try_pop() {
    int id = worker_id();
    return deques[id].pop_bottom();
  }


private:

  struct alignas(128) attempt { size_t val; };
  
  int num_deques;
  Deque<Job>* deques;
  attempt* attempts;
  int finished_flag;

  Job* try_steal(int id) {
    size_t target = (pbbs::hash32(id) + pbbs::hash32(attempts[id].val)) % num_deques;
    attempts[id].val++;
    return deques[target].pop_top();
  }

  template <typename F>
  Job* get_job(F finished) {
    if (finished()) return NULL;
    Job* job = try_pop();
    if (job) return job;
    int id = worker_id();
    int i = 0;
    while (1) {
      if (finished()) return NULL;
      job = try_steal(id);
      if (job) return job;
      if (i++ == 4*num_deques) {
	std::this_thread::sleep_for(std::chrono::nanoseconds(10000));
	i = 0;
      }
    }
  }

};

struct fork_join_scheduler {
  using Job = std::function<void()>;
  
  scheduler<Job>* sched;
  
  fork_join_scheduler() {
    sched = new scheduler<Job>;
  }

  ~fork_join_scheduler() {
    delete sched;
  }

  template <typename J>
  void run(J thunk, int num_threads=0) {
    Job job = [&] () {thunk(); sched->finish();};
    sched->run(&job,num_threads);
  }
    
  template <typename L, typename R>
  void pardo(L left, R right) {
    bool right_done = false;
    bool stolen = false;
    Job right_job = [&] () {
      stolen = true; right(); right_done = true;};
    sched->spawn(&right_job);
    left();
    if (!stolen) {
      Job* job = sched->try_pop();
      if (job != &right_job) sched->spawn(job);
      else { right(); return;}
    }
    auto finished = [&] () {return right_done;};
    sched->wait(finished);
  }
  
};
