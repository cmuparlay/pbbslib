#pragma once

#include "utilities.h"
#include <initializer_list>

#define hello foo

#ifdef CONCEPTS
template<typename T>
concept bool Seq =
  requires(T t, size_t u) {
  typename T::value_type;
  { t.size() } -> size_t;
  { t.slice() };
  { t[u] };
};

template<typename T>
concept bool Range =
  Seq<T> && requires(T t, size_t u) {
  { t[u] } -> typename T::value_type&;
  typename T::iterator;
};
#define SEQ Seq
#define RANGE Range
#else
#define SEQ typename
#define RANGE typename
#endif

namespace pbbs {

  template <typename Iterator>
  struct range {
  public:
    using value_type = typename std::iterator_traits<Iterator>::value_type;
    using iterator = Iterator;
    range() {};
    range(iterator s, iterator e) : s(s), e(e) {};
    value_type& operator[] (const size_t i) const {return s[i];}
    range slice(size_t ss, size_t ee) const {
      return range(s + ss, s + ee); }
    range slice() const {return range(s,e);};
    size_t size() const { return e - s;}
    iterator begin() const {return s;}
    iterator end() const {return e;}

    range<std::reverse_iterator<value_type*>>
    rslice(size_t ss, size_t ee) const {
      auto i = std::make_reverse_iterator(e);
      return range<decltype(i)>(i + ss, i + ee);
    }
    range<std::reverse_iterator<value_type*>>
    rslice() const {return rslice(0, std::distance(s,e));};

  private:
    iterator s;
    iterator e;
  };

  template <class Iter>
  range<Iter> make_range(Iter s, Iter e) {
    return range<Iter>(s,e);
  }

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

  // used so second template argument can be inferred
  template <class T, class F>
  delayed_sequence<T,F> delayed_seq (size_t n, F f) {
    return delayed_sequence<T,F>(n,f);
  }

  template <class F>
  auto seq (size_t n, F f) -> delayed_sequence<decltype(f(0)),F>
  {
    using T = decltype(f(0));
    return delayed_sequence<T,F>(n,f);
  }

  constexpr bool check_copy = false;

  template <typename T>
  struct sequence {
  public:
    using value_type = T;
    //using iterator = T*;

    sequence() { empty(); }

    // copy constructor
    sequence(const sequence& a) {
      if (check_copy && !a.is_small())
	cout << "copy constructor: len: " << a.size()
	     << " element size: " << sizeof(T) << endl;
      if (a.is_small()) val = a.val;
      else copy_from(a.val.large.s, a.val.large.n);
    }

    // move constructor
    sequence(sequence&& a) {
      val = a.val; a.empty();}

    // copy assignment
    sequence& operator = (const sequence& a) {
      if (check_copy && !a.is_small())
	cout << "copy assignment: len: " << a.size()
	     << " element size: " << sizeof(T) << endl;
      if (this != &a) {
	clear(); 
	if (a.is_small()) val = a.val;
	else copy_from(a.val.large.s, a.val.large.n);}
      return *this;
    }

    // move assignment
    sequence& operator = (sequence&& a) {
      if (this != &a) {clear(); val = a.val; a.empty();}
      return *this;
    }

    sequence(const size_t sz) {
      alloc(sz, true);}

    // only use if a is allocated by same allocator as sequence
    sequence(value_type* a, const size_t sz) {
      set(a, sz);
      // cout << "dangerous: " << size();
    };

    static sequence<T> no_init(const size_t sz) {
      sequence<T> r;
      r.alloc(sz, false);
      return r;
    };

    sequence(const size_t sz, value_type v) {
      T* start = alloc(sz, false);
      parallel_for(0, sz, [=] (size_t i) {
	  assign_uninitialized(start[i], (T) v);});
    };

    template <typename Func>
    sequence(const size_t sz, Func f) {
      T* start = alloc(sz, false);
      parallel_for(0, sz, [&] (size_t i) {
	  assign_uninitialized<T>(start[i], f(i));}, 1000);
    };

    sequence(std::initializer_list<value_type> l) {
      size_t sz = l.end() - l.begin();
      T* start = alloc(sz, true);
      size_t i = 0;
      for (T a : l) start[i++] = a;
    }

    template <typename Iter>
    sequence(range<Iter> a) {
      copy_from(a.begin(), a.size());
    }

    template <class F>
    sequence(delayed_sequence<T,F> a) {
      copy_from(a.begin(), a.size());
    }

    // Copies a Seq type 
    // Uses enable_if to avoid matching on integer argument, which creates
    // a sequece of the specified length
    //template <class Seq, typename std::enable_if_t<!std::is_integral<Seq>::value>>
    //sequence(Seq const &s) {
    //  copy_from(s.begin(), s.size());
    //}

    ~sequence() { clear();}

    range<value_type*> slice(size_t ss, size_t ee) const {
      return range<value_type*>(begin() + ss, begin() + ee);
    }

    range<std::reverse_iterator<value_type*>>
    rslice(size_t ss, size_t ee) const {
      auto iter = std::make_reverse_iterator(begin() + size());
      return range<decltype(iter)>(iter + ss, iter + ee);
    }

    range<std::reverse_iterator<value_type*>>
    rslice() const {return rslice(0, size());};

    range<value_type*> slice() const {
      return range<value_type*>(begin(), begin() + size());
    }

    // gives up ownership of space
    // only use if will be freed by same allocator as sequence
    value_type* to_array() {
      value_type* r = begin(); empty(); return r;}

    void clear() {
      if (size() != 0) {
        free(true);
	empty();
      }
    }

    void clear_no_destruct() {
      if (size() != 0) { free(false); empty();}} 
  
    value_type& operator[] (const size_t i) const {
      return begin()[i];
    }

    void swap(sequence& b) {
      std::swap(val.large.s, b.val.large.s); std::swap(val.large.n, b.val.large.n);
    }

    size_t size() const {
      if (is_small()) return val.small[15];
      return val.large.n;}

    value_type* begin() const {
      if (is_small()) return (T*) &val.small;
      return val.large.s;}

    value_type* end() const {return begin() + size();}

  private:

    // uses short string optimization (SSO)
    // currently only for char sequences
    union val_t {
      struct {
	T *s;
	size_t n;
      } large;
      char small[16];
    } val;

    void set(T* start, size_t sz) {
      val.large.n = sz;
      val.large.s = start;
    }
      
    // marks as empty
    void empty() {set(NULL, 0);}

    inline bool is_small(size_t sz) const {
      return std::is_same<T,char>::value && sz < 16 && sz > 0; }

    inline bool is_small() const {
      if (std::is_same<T,char>::value) {
	size_t sz = val.small[15];
	return (sz > 0 && sz < 16);
      }
      return false;
    }
    
    // allocate and set size
    value_type* alloc(size_t sz, bool init) {
      if (is_small(sz)) {
	val.small[15] = sz;
	return (T*) &val.small;
      }
      T* loc = (sz == 0) ? NULL :
	(init ? pbbs::new_array<T>(sz) : pbbs::new_array_no_init<T>(sz));
      set(loc, sz);
      return loc;
    }
    
    // free
    void free(bool destruct=true) {
      if (!is_small()) {
	if (destruct) // delete all elements
	  pbbs::delete_array<T>(val.large.s, val.large.n);
	else pbbs::free_array(val.large.s);
      }
    }

    template <class Iter>
    void copy_from(Iter a, size_t sz) {
      T* start = alloc(sz, false); 
      parallel_for(0, sz, [&] (size_t i) {
	  assign_uninitialized(start[i], a[i]);}, 1000);
    }

  };

  template <class Iter>
  bool slice_eq(range<Iter> a, range<Iter> b) {
    return a.begin() == b.begin();}

  template <class SeqA, class SeqB>
  bool slice_eq(SeqA a, SeqB b) { return false;}

  template <class Seq>
  auto to_sequence(Seq s) -> sequence<decltype(s[0])> {
    return sequence<decltype(s[0])>((size_t) s.size(), [&] (size_t i) {
	return s[i];});}
  
  std::ostream& operator<<(std::ostream& os, sequence<char> const &s)
  {
    // pad with a zero
    sequence<char> out(s.size()+1, [&] (size_t i) {
	return i == s.size() ? 0 : s[i];});
    os << out.begin();
    return os;
  }
}


