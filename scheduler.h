// from Arora, Blumofe, Plaxton
#include <omp.h>

using namespace std;

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

  Deque() : bot(-1) {
    age.pair.tag = 0;
    age.pair.top = -1;
  }
    
  void push_bottom(Job* node) {
    qidx local_bot = bot; // atomic load
    local_bot += 1;
    deq[local_bot] = node; // shared store
    fence();
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
    cas(&(age.unit), old_age.unit, new_age.unit); // cas
    if (old_age.unit == new_age.unit)
      return node;
    return NULL;
  }

  Job* pop_bottom() {
    age_t old_age, new_age;
    qidx local_bot = bot; // atomic load
    cout << "bot is " << local_bot << endl;
    if (local_bot == -1) 
      return NULL;
    local_bot = local_bot - 1;
    bot = local_bot; // shared store
    fence();
    Job* node = deq[local_bot]; // atomic load
    old_age.unit = age.unit; // atomic load
    cout << "here2 " << (node == NULL) << ", " << local_bot << ", " << old_age.pair.top << endl;
    if (local_bot > old_age.pair.top)
      return node;
    bot = -1; // shared store
    fence();
    new_age.pair.top = -1;
    new_age.pair.tag = old_age.pair.tag + 1;
    if (local_bot == old_age.pair.top) {
      cout << "check it out " << endl;
      if (cas(&(age.unit), old_age.unit, new_age.unit)) { // cas
	cout << "ooio" << endl;
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
  int num_deques;
  Deque<Job>* deques;
  int finished_flag;

public:
  scheduler() {
    num_deques = num_workers();
    deques = new Deque<Job>[num_deques];
    finished_flag = 0;
  }

  void run(Job* job) {
    //(*job)();
    cout << job << endl;
    deques[0].push_bottom(job);
    cout << deques[0].deq[0] << endl;
    auto finished = [&] () {return finished_flag > 0;};
    omp_set_num_threads(1);
    _Pragma("omp parallel");
    {
      _Pragma("omp single");
      cout << worker_id() << endl;
      wait(finished);
      _Pragma("omp single");
      cout << "end: " << worker_id() << endl;
    }
  }
    
  void spawn(Job* job) {
    int id = worker_id();
    deques[id].push_bottom[job];
  }

  template <typename F>
  void wait(F finished) {
    while (1) {
      cout << "try job: " << endl;
      Job* job = get_job(finished);
      if (!job) return;
      cout << "got job: " << job <<  endl;
      (*job)();
      cout << "end job" << endl;
    }
  }

  void finish() {finished_flag = 1;}

private:
  
  Job* try_pop() {
    int id = worker_id();
    cout << "worker id: " << id <<  endl;
    return deques[id].pop_bottom();
  }

  Job* try_steal() {
    int target = rand() % num_deques;
    return deques[target].pop_top();
  }

  template <typename F>
  Job* get_job(F finished) {
    if (finished()) return NULL;
    Job* job = try_pop();
    cout << "try pop: " << (job == NULL) <<  endl;
    if (job) return job;
    cout << "here: " << (job == NULL) <<  endl;
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
  void run(J thunk) {
    Job job = [&] () {cout << "foofoo" << endl; thunk(); sched->finish();};
    sched->run(&job);
  }
    
  template <typename L, typename R>
  void pardo(L left, R right) {
    int cnt = 0;
    Job right_job = [&] () {right(); cnt = 1;};
    sched->spawn(&right_job);
    left();
    auto finished = [&] () {return cnt == 1;};
    sched->wait(finished);
  }
  
};
