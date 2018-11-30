#include <omp.h>

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

  age_t age;
  qidx bot;
  Job* deq[100];

  inline bool cas(size_t* ptr, size_t oldv, size_t newv) {
    return __sync_bool_compare_and_swap(ptr, oldv, newv);
  }

  inline void fence() {
    std::atomic_thread_fence(std::memory_order_release);
  }

  Deque() : bot(0) {
    age.pair.tag = 0;
    age.pair.top = 0;
  }
    
  void push_bottom(Job* node) {
    qidx local_bot = bot; // atomic load
    deq[local_bot] = node; // shared store
    fence();
    local_bot += 1;
    bot = local_bot; // shared store
    fence(); // probably not needed
  }
  
  Job* pop_top() {
    age_t old_age, new_age;
    old_age.unit = age.unit; // atomic load
    qidx local_bot = bot; // atomic load
    if (local_bot <= old_age.pair.top)
      return NULL;
    Job* node = deq[old_age.pair.top]; // atomic load
    new_age.unit = old_age.unit;
    new_age.pair.top = new_age.pair.top + 1;
    if (cas(&(age.unit), old_age.unit, new_age.unit))  // cas
      return node;
    return NULL;
  }

  Job* pop_bottom() {
    age_t old_age, new_age;
    qidx local_bot = bot; // atomic load
    if (local_bot == 0) 
      return NULL;
    local_bot = local_bot - 1;
    bot = local_bot; // shared store
    fence();
    Job* node = deq[local_bot]; // atomic load
    old_age.unit = age.unit; // atomic load
    if (local_bot > old_age.pair.top)
      return node;
    bot = 0; // shared store
    fence();
    new_age.pair.top = 0;
    new_age.pair.tag = old_age.pair.tag + 1;
    if (local_bot == old_age.pair.top) {
      if (cas(&(age.unit), old_age.unit, new_age.unit)) { // cas
	return node;
      }
    }
    age.unit = new_age.unit; // shared store
    fence();
    return NULL;
  }
};

template <typename Job>
struct scheduler {

public:
  scheduler() {
    num_deques = 2*num_workers();
    deques = new Deque<Job>[num_deques];
    finished_flag = 0;
  }

  ~scheduler() {
    delete[] deques;
  }

  void run(Job* job, int num_threads = 0) {
    deques[0].push_bottom(job);
    auto finished = [&] () {return finished_flag > 0;};
    if (num_threads > 0 && num_threads < 2 * num_workers())
      omp_set_num_threads(num_threads);
    #pragma omp parallel
    wait(finished);
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
    }
    //while (!finished()) {
    //  std::yied
  }

  void finish() {finished_flag = 1;}

  Job* try_pop() {
    int id = worker_id();
    return deques[id].pop_bottom();
  }


private:

  int num_deques;
  Deque<Job>* deques;
  int finished_flag;

  Job* try_steal() {
    int target = rand() % num_deques;
    return deques[target].pop_top();
  }

  template <typename F>
  Job* get_job(F finished) {
    if (finished()) return NULL;
    Job* job = try_pop();
    if (job) return job;
    while (1) {
      if (finished()) return NULL;
      job = try_steal();
      if (job) return job;
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
