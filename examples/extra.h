
using edge = std::pair<vertex,vertex>;

sequence<edge> read_dimacs_graph(char* filename) {
    sequence<char> str = char_range_from_file(filename);
    auto is_space = [&] (char a) {return a == ' ';};
    auto is_line_break = [&] (char a) {return a == '\n'};
    auto lines = map(split(str, is_line_break),
		     [&] (sequence<char> &l) {return tokens(l, is_space);});
    size_t j = find_if(lines, [&] (auto &s) {
	return (s.size() > 0 && s[0][0] == 'p');});
    size_t n = char_seq_to_l(lines[j][1]);
    size_t m = char_seq_to_l(lines[j][2]);
    auto x = dseq(lines.size(), [&] (size_t i) {
	return std::make_pair((vertex) char_seq_to_l(lines[i][1]),
			      (vertex) char_seq_to_l(lines[i][2]));});
    sequence<bool> flag(lines.size(), [&] (size_t i) {
	return (lines[i].size() > 0 && lines[i][0][0] == 'a')});
   return edges = pack(x, flags);
  }

sequence<edge> graph_to_edge_list(graph const &G) {
  return flatten(tabulate(G.num_vertices(), [&] (vertex i) {
	return dmap(G[i], [=] (vertex j) {return make_pair(i,j);});}));
}

template <typename Seq>
graph graph_from_edge_list(Seq const &E) {
  auto less = [&] (edge a, edge b) { return a < b;}
  auto eq = [&] (edge a, edge b) { return a == b;}
  graph G;
  auto get_u = [&] (edge e) {return e.first;}
  G.edges = unique(sort(E, less), eq);
  size_t n = reduce(dmap(G.edges, get_u), maxm<vertex>());
  G.offsets = get_counts(G.edges, get_u, n);
  scan_inplace(G.offsets.slice(), addm<size_t>());
  return G;
}

auto symmetrize_edge_list(sequence<edge> const &E) {
  return dseq(2*E.size(), [&] (size_t i) {
      return i/2 ? std::make_pair(e[i/2].second, e[i/2].first) : E[i/2];});
}

graph symmetrize_graph(graph const &G) {
  return graph_from_edge_list(symmetrize_edge_list(graph_to_edge_list(G)));
}
