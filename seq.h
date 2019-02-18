#pragma once

#include "utilities.h"

namespace pbbs {
  
template <typename Iterator>
struct slice_t {
public:
  using value_type = typename std::iterator_traits<Iterator>::value_type;
  using iterator = Iterator;
  slice_t() {};
  slice_t(iterator s, iterator e) : s(s), e(e) {};
  value_type& operator[] (const size_t i) const {return s[i];}
  slice_t slice(size_t ss, size_t ee) const {
    return slice_t(s + ss, s + ee); }
  slice_t slice() const {return slice_t(s,e);};
  size_t size() const { return e - s;}
  iterator begin() const {return s;}
  iterator end() const {return e;}
private:
  iterator s; 
  iterator e; 
};

template <typename T, typename F>
struct delayed_sequence {
  using value_type = T;
  delayed_sequence(size_t n, F _f) : f(_f), s(0), e(n) {};
  delayed_sequence(size_t n, value_type v) : f([&] (size_t i) {return v;}), s(0), e(n) {};
  delayed_sequence(size_t s, size_t e, F _f) : f(_f), s(s), e(e) {};
  const value_type operator[] (size_t i) const {return (f)(i+s);}
  delayed_sequence<T,F> slice(size_t ss, size_t ee) const {
    return delayed_sequence<T,F>(s+ss,s+ee,f); }
  delayed_sequence<T,F> slice() const {
    return delayed_sequence<T,F>(s,e,f); }
  size_t size() const { return e - s;}
private:
  const F f;
  const size_t s, e;
};

template <typename T>
struct sequence {
public:
  using value_type = T;
  
  sequence() : s(NULL), n(0) {}

  // copy constructor
  sequence(const sequence& a) : n(a.n) {
    copy_here(a.s, a.n);
  }

  // move constructor
  sequence(sequence&& a)
    : s(a.s), n(a.n) {a.s = NULL; a.n = 0;}

  // copy assignment
  sequence& operator = (const sequence& a) {
    if (this != &a) {clear(); copy_here(a.s, a.n);}
    return *this;
  }

  // move assignment
  sequence& operator = (sequence&& a) {
    if (this != &a) {
      clear(); s = a.s; n = a.n; a.s = NULL; a.n = 0;}
    return *this;
  }

  sequence(const size_t n)
    : s(pbbs::new_array<T>(n)), n(n) {
    //if (n > 1000000000) cout << "make empty: " << s << endl;
  };

  sequence(value_type* a, const size_t n) : s(a), n(n) {
    //cout << "dangerous" << endl;
  };

  static sequence<T> no_init(const size_t n) {
    sequence<T> r;
    r.s = pbbs::new_array_no_init<T>(n);
    //if (n > 1000000000) cout << "make no init: " << r.s << endl;
    r.n = n;
    return r;
  };

  sequence(const size_t n, value_type v)
    : s(pbbs::new_array_no_init<T>(n, true)), n(n) {
    //if (n > 1000000000) cout << "make const: " << s << endl;
    auto f = [=] (size_t i) {new ((void*) (s+i)) value_type(v);};
    parallel_for(0, n, f);
  };

  template <typename Func>
  sequence(const size_t n, Func f)
    : s(pbbs::new_array_no_init<T>(n)), n(n) {
    //if (n > 1000000000) cout << "make func: " << s << endl;
    parallel_for(0, n, [&] (size_t i) {
	new ((void*) (s+i)) value_type(f(i));}, 1000);
  };

  template <typename Iter>
  sequence(slice_t<Iter> a) {
    copy_here(a.start(), a.size());
  }

  // template <class F>
  // sequence(delayed_sequence<T,F> a) {
  //   copy_here(a, a.size());
  // }
  
  ~sequence() { clear();}

  value_type& operator[] (const size_t i) const {return s[i];}

  slice_t<value_type*> slice(size_t ss, size_t ee) const {
    return slice_t<value_type*>(s + ss, s + ee);
  }

  slice_t<value_type*> slice() const {
    return slice_t<value_type*>(s, s + n);
  }

  void swap(sequence& b) {
    std::swap(s, b.s); std::swap(n, b.n);
  }

  size_t size() const { return n;}
  value_type* begin() const {return s;}
  value_type* end() const {return s + n;}

  // gives up ownership of space
  value_type* to_array() {
    value_type* r = s;  s = NULL; n = 0; return r;}

  void clear() {
    if (s != NULL) {
      //if (n > 1000000000) cout << "delete: " << s << endl;
      pbbs::delete_array<T>(s, n);
      s = NULL; n = 0;
    }
  }

private:
  template <class Seq>
  void copy_here(Seq a, size_t n) {
    s = pbbs::new_array_no_init<T>(n, true);
    if (n > 0) {
      //cout << "Yikes, copy: " << s << endl;
      //abort();  
    }
    parallel_for(0, n, [&] (size_t i) {
	pbbs::assign_uninitialized(s[i], a[i]);});
  }
  
  T *s; 
  size_t n;
};

// used so second template argument can be inferred
template <class T, class F>
delayed_sequence<T,F> delayed_seq (size_t n, F f) {
  return delayed_sequence<T,F>(n,f);
}

template <class Iter>
slice_t<Iter> make_slice(Iter s, Iter e) {
  return slice_t<Iter>(s,e);
}
}
