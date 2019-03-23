// This code is part of the project "A Simple Parallel Cartesian Tree
// Algorithm and its Application to Parallel Suffix Tree
// Construction", ACM Transactions on Parallel Computing, 2014
// (earlier version appears in ALENEX 2011).  
// Copyright (c) 2014-2019 Julian Shun and Guy Blelloch
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "sequence.h"
#include "get_time.h"
#include "suffix_array.h"
#include "integer_sort.h"
#include "lcp.h"
#include "cartesian_tree.h"
#include "histogram.h"

namespace pbbs {


  template <class Uint>
  struct suffix_tree {
    struct edge {
      Uint child;
      unsigned char c;
      bool is_leaf;
    };

    using Pair = std::pair<Uint,Uint>;

    struct node {
      Uint lcp;
      Uint location;
      Uint offset;
    };

    struct ct_node {
      Uint parent;
      Uint value;};
    
    sequence<edge> Edges;
    sequence<node> Nodes;
    sequence<ct_node> CT_Nodes;
    Uint n;
    Uint num_edges;
    
    void set_cluster_root(size_t i) {
      auto root = CT_Nodes[i].parent;
      while (root != 0 &&
	     CT_Nodes[CT_Nodes[root].parent].value == CT_Nodes[root].value)
	root = CT_Nodes[root].parent;
      CT_Nodes[i].parent = root;
    }

    suffix_tree(sequence<unsigned char> const &S) {
      timer t("suffix tree", true);
      n = S.size();
      cout << n << endl;
      sequence<Uint> SA = suffix_array<Uint>(S);
      t.next("suffix array");
      sequence<Uint> LCPs_ = LCP(S, SA);
      sequence<Uint> LCPs(n, [&] (size_t i) {
	  return (i==0) ? 0 : LCPs_[i-1];}); // shift by 1
      LCPs_.clear();
      t.next("LCP");
      suffix_tree_from_SA_LCP(SA, LCPs, S);
    }      

    void suffix_tree_from_SA_LCP(sequence<Uint> const &SA,
				 sequence<Uint> const &LCPs,
				 sequence<unsigned char> const &S) {
      timer t("suffix tree", true);
      n = SA.size();
      sequence<Uint> Parents = cartesian_tree(LCPs);
      t.next("Cartesian Tree");
      
      //for (size_t i=0; i < n; i++)
      //cout << i << ", " << LCPs[i] << ", " << Parents[i] << endl;

      // A cluster is a set of connected nodes in the CT with the same LCP
      // Each cluster needs to be converted into a single node.
      // The following marks the roots of each cluster and has Roots point to self
      sequence<Uint> Roots(n);
      sequence<bool> is_root(n, [&] (Uint i) {
	  bool is_root_i = (i == 0) || (LCPs[Parents[i]] != LCPs[i]);
	  Roots[i] = is_root_i ? i : Parents[i];
	  return is_root_i;
	});

      // this shortcuts each node to point to its root
      parallel_for(0, n, [&] (Uint i) {
	  if (!is_root[i]) {
	    Uint root = Roots[i];
	    while (!is_root[root]) root = Roots[root];
	    Roots[i] = root;
	  }});
      t.next("Cluster roots");

      sequence<Uint> new_labels;
      Uint num_internal;
      std::tie(new_labels, num_internal) = enumerate<Uint>(is_root);
      t.next("Relabel roots");
      cout << "num internal: " << num_internal << endl;
      
      // interleave potential internal nodes and leaves
      auto edges = delayed_seq<Pair>(2*n, [&] (size_t j) {
	  if (j & 1) { // is leaf (odd)
	    Uint i = j/2;
	    // for each element of SA find larger LCP on either side
	    // the root of it will be the parent in the suffix tree
	    Uint parent = (i==n-1) ? i : (LCPs[i] > LCPs[i+1] ? i : i+1);
	    return Pair(new_labels[Roots[parent]], j);
	  } else { // is internal node (even)
	    Uint i = j/2;
	    Uint parent = Parents[i];
	    return Pair(new_labels[Roots[parent]], 2 * new_labels[i]);
	  }
	});

      // keep if leaf or root internal node (except the overall root).
      sequence<bool> flags(2*n, [&] (size_t j) {
	  return (j & 1)  || (is_root[j/2] && (j != 0));});

      sequence<Pair> all_edges = pack(edges, flags);
      t.next("generate edges");
      flags.clear();
      Roots.clear();
      new_labels.clear();
      
      auto get_first = [&] (Pair p) {return p.first;};

      sequence<Pair> sorted_edges =
	integer_sort(all_edges, get_first, log2_up(num_internal));
      t.next("Sort edges");
      // sort all edges by parent

      sequence<Uint> offsets = get_counts<Uint>(sorted_edges, get_first, num_internal);
      //sequence<size_t> h = histogram<size_t>(offsets, 200);
      //int i = 2;
      //int total = 0;
      //while (h[i] > 0) cout << i  << " : " << h[i] << ", " << (total += h[i++]) << endl;
      
      scan_inplace(offsets.slice(), addm<Uint>());
      t.next("Get Counts");

      cout << "n = " << n << " m = " << num_internal << endl;
      
      //for (size_t i=0; i < sorted_edges.size(); i++)
      //  cout << sorted_edges[i].first << ", " << sorted_edges[i].second << endl;

      // tag edges with character from s
      sequence<Uint> root_indices = pack_index<Uint>(is_root);
      Edges = map<edge>(sorted_edges, [&] (Pair p) {
	  Uint child = p.second;
	  Uint i = child/2;
	  bool is_leaf = child & 1;
	  int depth = (is_leaf ?
		       ((i==n-1) ? LCPs[i] : std::max(LCPs[i], LCPs[i+1])) :
		       LCPs[Parents[root_indices[i]]]);
	  int start = is_leaf ? SA[i] : SA[root_indices[i]];
	  //cout << i << ", " << depth << ", " << is_leaf << ", " << SA[i] << endl;
	  edge e = {i, S[start+depth], is_leaf};
	  return e;
	});
      t.next("project edges");
      sorted_edges.clear();
      Parents.clear();

      //for (size_t i=0; i < Edges.size(); i++)
      //  cout << Edges[i].child << ", " << Edges[i].c << ", " << Edges[i].is_leaf << endl;

      Nodes = sequence<node>(num_internal, [&] (size_t i) {
	  Uint lcp = LCPs[root_indices[i]];
	  Uint offset = offsets[i];
	  Uint location = SA[root_indices[i]];
	  node r = {lcp, location, offset};
	  //cout << i << ", " << offset << ", " << location << ", " << lcp << endl;
	  return r;
	});
      t.next("Make nodes");
    }

    // enum ftype {leaf_t, internal_t, none_t};
    
    // pair<Uint,char> find_child(Uint i, uchar c) {
    //   node node = Nodes[i];
    //   Uint start = node.offset;
    //   Uint end = Nodes[i+1].offset;
    //   for (Uint j=start; j < end; j++) {
    // 	if (Edges[j].c == c)
    // 	  if (Edges[j].is_leaf) return make_pair(Edges[j].child, leaf_t);
    // 	  else return make_pair(Edges[j].child, internal_t);
    //   }
    //   return make_pair(0, none_t);
    // }

    // pair<Uint,char> find(char* s) {
    //   Uint i = 0;
    //   ftype a;
    //   do {
    // 	std::bind(i,a) = find_child(i, *s);
    // 	if (a == none_t) return pair(i,none_t);
    // 	return pair(i,none_t);
    //   }
    // }	
  };
}
