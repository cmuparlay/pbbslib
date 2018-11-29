// from Arora, Blumofe, Plaxton

template <typename Job>
struct Deque {
  using qidx = int;
  age_t age;
  qidx bot;
  Job*[100] deq;

  // use unit for atomic access
  union age_t {
    struct {
      int tag;
      q_idx top;
    } pair;
    size_t unit;
  };

  inline bool cas(size_t* ptr, size_t old, size_t new) {
    return __sync_bool_compare_and_swap(ptr, old, new);
  }

  inline void fence() {
    std::atomic_thread_fence(std::memory_order_release);
  }

  Deque() : bot(0) { age.unit = 0; }
    
  void push_bottom(Job node) {
    qidx local_bot = bot; // atomic load
    deq[local_bot] = node; // shared store
    fence();
    bot = local_bot + 1; // shared store
    fence(); // probably not needed
  }
  
  Job* pop_top() {
    age_t old_age.unit = age.unit; // atomic load
    qidx local_bot = bot; // atomic load
    if (local_bot <= old_age.pair.top)
      return NULL;
    Job* node = deq[old_age.pair.top]; // atomic load
    age_t new_age = old_age;
    new_age.pair.top = new_age.pair.top + 1;
    cas(&(age.unit), old_age.unit, new_age.unit); // cas
    if (old_age.unit == new_age.unit)
      return node;
    return NULL;
  }

  Job* pop_bottom() {
    qidx local_bot = bot; // atomic load
    if (local_bot == 0) 
      return NULL;
    local_bot = local_bot - 1;
    bot = local_bot; // shared store
    fence();
    Job* node = deq[local_bot]; // atomic load
    age_t old_age.unit = age.unit; // atomic load
    if (local_bot > old_age.pair.top)
      return node;
    bot = 0; // shared store
    fence();
    new_age.pair.top = 0;
    new_age.pair.tag = old_age.pair.tag + 1;
    if (local_bot == old_age.pair.top) {
      cas(&(age.unit), old_age.unit, new_age.unit); // cas
      if (old_age.unit == new_age.unit)
	return node;
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
    deques = new Deqeue<Job>[num_deques];
    finished_flag = 0;
  }

  void run(Job node) {
    deques[id].push_bottom[0];
    auto finished = [&] () {return finished_flag > 0;}
#pragma openmp parallel
    {
      wait(finished);
    }
  }
    
  void spawn(Job node) {
    int id = worker_id();
    deques[id].push_bottom[node];
  }

  template <typenae F>
  void wait(F finished) {
    while (1) {
      Job* node = get_job(finished);
      if (!node) return;
      job();
    }
  }

  void finish() {finish_flag = 1;}

private:
  
  Job* try_pop() {
    int id = worker_id();
    return deques[id].pop_bottom();
  }

  Job* try_steal() {
    int target = rand() % num_deques;
    return deques[target].pop_top();
  }

  template <typename F>
  Job* get_job(F finished) {
    if (finished()) return NULL;
    Job* node = try_pop();
    if (node) return node;
    while (1) {
      if (finished()) return NULL;
      node = try_steel();
      if (node) return node;
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
    Job job = [&] () {thunk(); sched->finish();};
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
