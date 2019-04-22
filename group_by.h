#pragma once

#include "sequence.h"

namespace pbbs {
  
  template <class Seq, class Comp>
  auto group_by(Seq &&S, Comp less) {
    using KV = typename std::remove_reference<Seq>::type::value_type;
    using K = typename KV::first_type;
    using V = typename KV::second_type;
    timer t("group by", false);
    size_t n = S.size();
  
    auto pair_less = [&] (std::pair<K,V> const &a, std::pair<K,V> const &b) {
      return less(a.first, b.first);};

    auto sorted = pbbs::sample_sort(std::forward<Seq>(S), pair_less, true);
    //auto sorted = pbbs::sample_sort(S, pair_less);
    t.next("sort");
      
    pbbs::sequence<bool> Fl(n, [&] (size_t i) {
	return (i==0) || pair_less(sorted[i-1], sorted[i]);});
    t.next("flags");
  
    auto idx = pack_index<size_t>(Fl);
    t.next("pack index");
  
    auto r = tabulate(idx.size(), [&] (size_t i) {
	size_t m = ((i==idx.size()-1) ? n : idx[i+1]) - idx[i];
	return std::make_pair(std::move(sorted[idx[i]].first),
			      sequence<size_t>(m, [&] (size_t j) {
				  return sorted[idx[i] + j].second;}));});
    t.next("make pairs");
    return r;
  }

  template <class T>
  struct compare {
    bool operator()(const T& a, const T& b) const {return a < b;}};

  template <>
  struct compare<char*> {
    bool operator()(char* a, char* b) const {
      return strcmp(a, b) < 0;}};

  template <>
  struct compare<sequence<char>> {
    bool operator()(sequence<char> const &s1, sequence<char> const &s2) const {
      size_t m = std::min(s1.size(), s2.size());
      size_t i = 0;
      char* ss1 = s1.begin();
      char* ss2 = s2.begin();
      while (i < m && ss1[i] == ss2[i]) i++;
      return (i < m) ? (ss1[i] < ss2[i]) : (s1.size() < s2.size());
    }
  };

  template <class Seq>
  auto group_by(Seq &&S) {
    using KV = typename std::remove_reference<Seq>::type::value_type;
    using K = typename KV::first_type;
    return group_by(std::forward<Seq>(S), compare<K>());
  }

}
