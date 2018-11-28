// from Arora, Blumofe, Plaxton

template <typename Job>
struct Deque {
  using qidx = int;
  age_t age;
  qidx bot;
  Job*[100] deq;

  struct age_pair {
    int tag;
    q_idx top;
  };

  // use unit for atomic access
  union age_t {
    age_pair pair;
    size_t unit;
  };

  Deque() : bot(0) {
    age.unit = 0;
    // probably clear queue, but need default value
  }
      
  // shared stores are marked since appropriate memory barriers
  // should be used after each
    
  void push_bottom(Job node) {
    qidx local_bot = bot; // atomic load
    deq[local_bot] = node; // store
    bot = local_bot + 1; // store
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
    bot = local_bot; // store
    Job* node = deq[local_bot]; // atomic load
    age_t old_age.unit = age.unit; // atomic load
    if (local_bot > old_age.pair.top)
      return node;
    bot = 0; // store
    new_age.pair.top = 0;
    new_age.pair.tag = old_age.pair.tag + 1;
    if (local_bot == old_age.pair.top) {
      cas(&(age.unit), old_age.unit, new_age.unit); // cas
      if (old_age.unit == new_age.unit)
	return node;
    }
    age.unit = new_age.unit; // store
    return NULL;
  }
};

template <typename Job>
struct scheduler {
  int num_deques;
  Deque<Job>* deques;

  scheduler() {
    num_deques = num_workers();
    deques = new Deqeue<Job>[num_deques];
  }

  push(Job node) {
    int id = worker_id();
    deques[id].push_bottom[node];
  }

  Job* try_pop() {
    int id = worker_id();
    return deques[id].pop_bottom();
  }

  Job* try_steel() {
    int target = rand() % num_deques;
    return deques[target].pop_top();
  }
}
    
