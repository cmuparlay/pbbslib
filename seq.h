#pragma once

#include "utilities.h"

namespace pbbs {
  
template <typename Iterator>
struct slice_t {
public:
  using T = typename std::iterator_traits<Iterator>::value_type;
  using Iter = Iterator;
  slice_t() {}
  slice_t(Iter s, Iter e) : s(s), e(e) {};
  T& operator[] (const size_t i) const {return s[i];}
  slice_t slice(size_t ss, size_t ee) const {
    return slice_t(s + ss, s + ee); }
  slice_t slice() const {return slice_t(s,e);};
  size_t size() const { return e - s;}
  Iter begin() const {return s;}
  Iter end() const {return e;}
private:
  Iter s; 
  Iter e; 
};

template <typename E, typename F>
struct delayed_sequence {
  using T = E;
  delayed_sequence(size_t n, F _f) : f(_f), s(0), e(n) {};
  delayed_sequence(size_t n, T v) : f([&] (size_t i) {return v;}), s(0), e(n) {};
  delayed_sequence(size_t s, size_t e, F _f) : f(_f), s(s), e(e) {};
  const T operator[] (size_t i) const {return (f)(i+s);}
  delayed_sequence<T,F> slice(size_t ss, size_t ee) const {
    return delayed_sequence<T,F>(s+ss,s+ee,f); }
  delayed_sequence<T,F> slice() const {
    return delayed_sequence<T,F>(s,e,f); }
  size_t size() const { return e - s;}
private:
  const F f;
  const size_t s, e;
};

template <typename E>
struct sequence {
public:
  using T = E;

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
    : s(pbbs::new_array<E>(n)), n(n) {
    //if (n > 1000000000) cout << "make empty: " << s << endl;
  };

  sequence(T* a, const size_t n) : s(a), n(n) {
    //cout << "dangerous" << endl;
  };

  static sequence<E> no_init(const size_t n) {
    sequence<E> r;
    r.s = pbbs::new_array_no_init<E>(n);
    //if (n > 1000000000) cout << "make no init: " << r.s << endl;
    r.n = n;
    return r;
  };

  sequence(const size_t n, T v)
    : s(pbbs::new_array_no_init<E>(n, true)), n(n) {
    //if (n > 1000000000) cout << "make const: " << s << endl;
    auto f = [=] (size_t i) {new ((void*) (s+i)) T(v);};
    parallel_for(0, n, f);
  };

  template <typename Func>
  sequence(const size_t n, Func f)
    : s(pbbs::new_array_no_init<E>(n)), n(n) {
    //if (n > 1000000000) cout << "make func: " << s << endl;
    parallel_for(0, n, [&] (size_t i) {
	new ((void*) (s+i)) T(f(i));}, 1000);
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

  T& operator[] (const size_t i) const {return s[i];}

  slice_t<T*> slice(size_t ss, size_t ee) const {
    return slice_t<T*>(s + ss, s + ee);
  }

  slice_t<T*> slice() const {
    return slice_t<T*>(s, s + n);
  }

  void swap(sequence& b) {
    std::swap(s, b.s); std::swap(n, b.n);
  }

  size_t size() const { return n;}
  T* begin() const {return s;}
  T* end() const {return s + n;}

  // gives up ownership of space
  T* to_array() {
    T* r = s;  s = NULL; n = 0; return r;}

  void clear() {
    if (s != NULL) {
      //if (n > 1000000000) cout << "delete: " << s << endl;
      pbbs::delete_array<E>(s, n);
      s = NULL; n = 0;
    }
  }

private:
  template <class Seq>
  void copy_here(Seq a, size_t n) {
    s = pbbs::new_array_no_init<E>(n, true);
    if (n > 0) {
      //cout << "Yikes, copy: " << s << endl;
      //abort();  
    }
    parallel_for(0, n, [&] (size_t i) {
	pbbs::assign_uninitialized(s[i], a[i]);});
  }
  
  E *s; 
  size_t n;
};

// used so second template argument can be inferred
template <class E, class F>
delayed_sequence<E,F> delayed_seq (size_t n, F f) {
  return delayed_sequence<E,F>(n,f);
}

template <class Iter>
slice_t<Iter> make_slice(Iter s, Iter e) {
  return slice_t<Iter>(s,e);
}
}
