#include "sequence.h"
#include "get_time.h"
#include "ligra.h"
#include "parse_command_line.h"

using namespace pbbs;

// **************************************************************
//    BFS edge_map structure
// **************************************************************

using vertex = ligra::vertex;

struct BFS_F {
  sequence<vertex> Parents;
  vertex n;
  BFS_F(vertex n) : Parents(sequence<vertex>(n, n)), n(n) { }
  inline bool updateAtomic (vertex s, vertex d) {
    return atomic_compare_and_swap(&Parents[d], n , s); }
  inline bool update (long s, long d) {
    Parents[d] = s; return true;}
  inline bool cond (long d) { return (Parents[d] == n); }
};

// **************************************************************
//    Run BFS, returning number of levels, and number of vertices
//    visited
// **************************************************************

std::pair<size_t,size_t> bfs(ligra::graph const &g, vertex start) {
  auto BFS = BFS_F(g.num_vertices());
  BFS.Parents[start] = start;
  ligra::vertex_subset frontier(start); //creates initial frontier
  size_t levels = 0, visited = 0;
  while(!frontier.is_empty()) { //loop until frontier is empty
    visited += frontier.size();
    levels++;
    frontier = ligra::edge_map(g, frontier, BFS);
  }
  return std::make_pair(levels, visited);
}

// **************************************************************
//    Main
// **************************************************************

int main (int argc, char *argv[]) {
  commandLine P(argc, argv,
     "[-r <rounds>] [-t <sparse_dense_ratio>] [-s <source>] filename");
  int rounds = P.getOptionIntValue("-r", 1);
  ligra::sparse_dense_ratio = P.getOptionIntValue("-t", 10);
  int start = P.getOptionIntValue("-s", 0);
  char* filename = P.getArgument(0);
  timer t("BFS");
  auto g = ligra::read_graph(filename);
  t.next("read and parse graph");

  size_t levels, visited;
  for (int i=0; i < rounds; i++) {
    std::tie(levels, visited) = bfs(g, start);
    t.next("calculate bfs");
  }
  cout << levels << " levels in BFS, "
       << visited << " vertices visited" << endl;
}

