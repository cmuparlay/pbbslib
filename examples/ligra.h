#include "sequence.h"
#include "get_time.h"
#include "strings/string_basics.h"

namespace ligra {
  
  using namespace pbbs;

  using vertex = uint;
  using edge_index = size_t;

  // **************************************************************
  //    sparse compressed row representation of a graph
  // **************************************************************

  struct graph {
    using NGH = range<vertex*>;
    sequence<edge_index> offsets;
    sequence<vertex> edges;
    size_t num_vertices() const {return offsets.size();}
    size_t num_edges() const {return edges.size();}
    NGH operator[] (const size_t i) const {
      size_t end = (i==num_vertices()-1) ? num_edges() :offsets[i+1];
      return edges.slice(offsets[i], end);
    }
  };

  // **************************************************************
  //    vertex_subset
  // **************************************************************

  struct vertex_subset {
    bool is_dense;
    sequence<bool> flags;
    sequence<vertex> indices;
    vertex_subset(vertex v) : is_dense(false), indices(singleton(v)) {}
    vertex_subset(sequence<vertex> indices)
      : is_dense(false), indices(std::move(indices)) {}
    vertex_subset(sequence<bool> flags)
      : is_dense(true), flags(std::move(flags)) {}
    size_t size() const {
      return is_dense ? count(flags, true) : indices.size();}
    bool is_empty() {return size() == 0;}
    sequence<bool> get_flags(size_t n) const {
      if (is_dense) return flags;
      sequence<bool> r(n, false);
      parallel_for (0, indices.size(), [&] (size_t i) {
	  r[indices[i]] = true;});
      return r;
    }
    sequence<vertex> get_indices() const {
      if (!is_dense) return indices;
      return pack_index<vertex>(flags);
    }
  };

  // **************************************************************
  //    read a graph stored in CSR format (ascii)
  //    expects: <filetype> <number vertices> <number edges> offset_list edge_list
  // **************************************************************

  graph read_graph(char* filename) {
    sequence<char> str = char_range_from_file(filename);
    auto is_space = [&] (char a) {return a == ' ' || a == '\n';};
    auto words = tokenize(str, is_space);
    size_t n = atol(words[1]); // num_vertices
    size_t m = atol(words[2]);  // num_edges
    if (3 + n + m != words.size() && (3 + n + 2*m != words.size()))
      throw std::runtime_error("Badly formatted file in read_graph");
    graph g;
    g.offsets = map(words.slice(3,3+n), [&] (auto &s) {
	return (edge_index) atol(s);});
    g.edges = map(words.slice(3+n,3+n+m), [&] (auto &s)  {
	return (vertex) atol(s);});
    return g;
  }

  // **************************************************************
  //    edge_map
  // **************************************************************

  size_t sparse_dense_ratio = 10;
  
  template <typename mapper>
  vertex_subset edge_map(graph const &g, vertex_subset const &vs, mapper &m) {

    auto edge_map_sparse = [&] (sequence<vertex> const &idx) {
      size_t n = g.num_vertices();
      //cout << "sparse: " << idx.size() << endl;

      auto offsets = map(idx, [&] (vertex v) { return g[v].size();});
    
      // Find offsets to write the next frontier for each v in this frontier
      size_t total = pbbs::scan_inplace(offsets.slice(), addm<vertex>());
      pbbs::sequence<vertex> next(total);

      parallel_for(0, idx.size(), [&] (size_t i) {
	  auto v = idx[i];
	  auto ngh = g[v];
	  auto o = offsets[i];
	  parallel_for(0, ngh.size(), [&] (size_t j) {
	      next[o + j] = (m.cond(ngh[j]) &&
			     m.updateAtomic(v, ngh[j])) ? ngh[j] : n;
	    }, 1000);
	});

      auto r = filter(next, [&] (vertex i) {return i < n;});
      return vertex_subset(std::move(r));
    };

    auto edge_map_dense = [&] (sequence<bool> const &flags) {
      //cout << "dense: " << count(flags,true) << endl;

      sequence<bool> out_flags(flags.size(), [&] (size_t d) {
	  auto in_nghs = g[d];
	  bool r = false;
	  for (size_t i=0; i < in_nghs.size(); i++) {
	    if (!m.cond(d)) break;
	    auto s = in_nghs[i];
	    if (flags[s] && m.update(s, d)) r = true;
	  }
	  return r;
	});

      return vertex_subset(std::move(out_flags));
    };

    if (vs.is_dense)
      if (vs.size() > g.num_vertices()/sparse_dense_ratio)
    	return edge_map_dense(vs.flags);
      else return edge_map_sparse(vs.get_indices());
    else
      if (reduce(dmap(vs.indices, [&] (vertex v) { return g[v].size();}),
		 addm<vertex>()) > g.num_edges()/sparse_dense_ratio)
	return edge_map_dense(vs.get_flags(g.num_vertices()));
      else return edge_map_sparse(vs.indices);
    
  }

  // **************************************************************
  //    vertex_map and vertex_filter
  // **************************************************************

  template <class F>
  void vertex_map(vertex_subset vs, F f) {
    if (vs.is_dense) 
      parallel_for(0, vs.flags.size(), [&] (size_t i) {
	  if (vs.flags[i]) f(i);});
    else
      parallel_for(0, vs.indices.size(), [&] (size_t i) {
	  f(vs.indices[i]);});
  }

  template <class F>
  vertex_subset vertex_filter(vertex_subset vs, F f) {
    if (vs.is_dense)
      return
	vertex_subset(tabulate(vs.flags.size(), [&] (size_t i) {
	      return vs.flags[i] && (bool) f(i);}));
    else
      return vertex_subset(filter(vs.indices, f));
  }
}
