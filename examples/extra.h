
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
  auto flipped = map(E, [&] (auto e) {
      return std::make_pair(e.second, e.first);});
  return append(Ef, flipped);
}
  
graph graph_from_edges(sequence<edge> const &E) {
  auto less = [&] (edge a, edge b) { return a < b;}
  auto eq = [&] (edge a, edge b) { return a == b;}
  graph G;
  G.edges = unique(sort(E, less), eq);
  auto is_start = tabulate(edges.size(), [&] (size_t i) {
      return (i==0) || (edges[i].first != edges[i-1].first);});
  G.offsets = pack_index<size_t>(is_start);
  return G;
}
