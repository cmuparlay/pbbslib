
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


sequence<edge> remove_self_edges(sequence<edge> const &E) {
  return filter(E, [&] (auto e) {return e.first != e.second;});}

sequence<edge> symmetrize_graph(sequence<edge> const &E) {
  auto flipped = dmap(E, [&] (auto e) {
      return std::make_pair(e.second, e.first);});
  return append(Ef, flipped);
}
  
graph graph_from_edges(sequence<edge> const &E) {
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
