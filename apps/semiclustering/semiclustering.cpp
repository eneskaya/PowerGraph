#include <vector>
#include <string>
#include <fstream>

#include <graphlab.hpp>

int C_MAX = 5;
int V_MAX = 3;
double F_B = 0.1;

size_t ITERATIONS = 5;

class SemiCluster
{
public:
  SemiCluster() : semiScore(1.0), members(){};

  SemiCluster(const SemiCluster &rhs)
  {
    semiScore = rhs.semiScore;
    members = rhs.members;
  }

  SemiCluster(float semiScore, std::vector<graphlab::vertex_id_type> members)
  {
    this->semiScore = semiScore;
    this->members = members;
  }

  float semiScore;
  std::vector<graphlab::vertex_id_type> members;

  /**
     * Add a new vertex to this cluster, by appending to members.
     * Also update the semiScore.
     * For this, we look into the edges of the new vertex.
     * Iterating over the edges, for each edge u, we will check wether it is in the members list already.
     * If yes, the weight of that edge is added to innerWeight.
     * If no, the weight of that edge is added to outerWeight.
     */
  bool addToCluster(graphlab::vertex_id_type newVertexId, std::vector<std::pair<graphlab::vertex_id_type, float>> edges)
  {
    // abort if Vmax is reached
    if (members.size() == V_MAX || std::find(members.begin(), members.end(), newVertexId) != members.end())
      return false;

    float innerWeight = 0;
    float outerWeight = 0;

    members.push_back(newVertexId);

    for (std::pair<int, float> e : edges)
    {
      int u = e.first;
      float weight = e.second;

      // If u is not in the in the members list, it is an outEdge (outside of cluster)
      // https://stackoverflow.com/questions/571394/how-to-find-out-if-an-item-is-present-in-a-stdvector
      if (std::find(members.begin(), members.end(), u) == members.end())
      {
        outerWeight += weight;
      }
      else
      {
        innerWeight += weight;
      }
    }

    // compute S_c
    semiScore = (innerWeight - F_B * outerWeight) / ((members.size() * (members.size() - 1)) / 2);
    return true;
  }

  bool operator<(const SemiCluster &rhs) const { return semiScore < rhs.semiScore; }
  bool operator>(const SemiCluster &rhs) const { return semiScore > rhs.semiScore; }

  void save(graphlab::oarchive &oarc) const
  {
    oarc << semiScore << members;
  }

  void load(graphlab::iarchive &iarc)
  {
    iarc >> semiScore >> members;
  }
};

struct SemiClusterContainer
{
  SemiClusterContainer() : clusters(), neighbors(){};

  SemiClusterContainer(const SemiClusterContainer &rhs)
  {
    clusters = rhs.clusters;
    neighbors = {};
  }

  std::vector<SemiCluster> clusters;
  std::vector<std::pair<graphlab::vertex_id_type, float>> neighbors;

  void save(graphlab::oarchive &oarc) const
  {
    oarc << clusters;
  }

  void load(graphlab::iarchive &iarc)
  {
    iarc >> clusters;
  }

  SemiClusterContainer &operator+=(SemiClusterContainer const &other)
  {
    for (SemiCluster c : other.clusters)
    {
      clusters.push_back(c);
    }
    for (std::pair<graphlab::vertex_id_type, float> e : other.neighbors)
    {
      neighbors.push_back(e);
    }

    std::sort(clusters.begin(), clusters.end());
    std::reverse(clusters.begin(), clusters.end());

    if (clusters.size() > V_MAX)
    {
      clusters.resize(V_MAX);
    }

    return *this;
  }
};

typedef float edge_data_type;
typedef SemiClusterContainer vertex_data_type;
typedef SemiClusterContainer gather_type;

typedef graphlab::distributed_graph<vertex_data_type, edge_data_type> graph_type;

void init_vertex(graph_type::vertex_type &vertex)
{
  SemiCluster s;
  s.members.push_back(vertex.id());

  SemiClusterContainer sc;
  sc.clusters.push_back(s);

  vertex.data() = sc;
}

std::string printCluster(SemiCluster const &data)
{
  std::stringstream out;

  out << "score: " << data.semiScore << ", members: "
      << " {";
  for (int m : data.members)
  {
    out << " " << m << " ";
  }
  out << "} ";

  return out.str();
}

struct cluster_reducer
{
  std::vector<SemiCluster> clusters;
  graph_type::vertex_id_type vertexId;

  void save(graphlab::oarchive &oarc) const
  {
    oarc << clusters;
  }

  void load(graphlab::iarchive &iarc)
  {
    iarc >> clusters;
  }

  static cluster_reducer start(const graph_type::vertex_type &v)
  {
    cluster_reducer r;
    r.vertexId = v.id();

    for (SemiCluster c : v.data().clusters)
    {
      r.clusters.push_back(c);
    }

    return r;
  }

  cluster_reducer &operator+=(const cluster_reducer &other)
  {
    for (SemiCluster c : other.clusters)
    {
      clusters.push_back(c);
    }

    return *this;
  }
};

// The factorized vertex program
class semiclustering : public graphlab::ivertex_program<graph_type, gather_type>, public graphlab::IS_POD_TYPE
{
public:
  bool has_changed;

  edge_dir_type gather_edges(icontext_type &context, const vertex_type &vertex) const
  {
    return graphlab::ALL_EDGES;
  }

  // Gather the semi-clusters of all neighbours
  // This method is called once per edge
  gather_type gather(icontext_type &context, const vertex_type &vertex, edge_type &edge) const
  {
    // In the SemiClusterContainer, gather all SemiClusters of the neighbourhood
    gather_type sc;
    std::pair<graphlab::vertex_id_type, float> n;

    if (edge.target().id() == vertex.id())
    {
      n.first = edge.source().id();
      for (SemiCluster c : edge.source().data().clusters)
      {
        sc.clusters.push_back(c);
      }
    }
    else
    {
      n.first = edge.target().id();
      for (SemiCluster c : edge.target().data().clusters)
      {
        sc.clusters.push_back(c);
      }
    }

    n.second = edge.data();
    sc.neighbors.push_back(n);

    return sc;
  }

  // Update the cluster list of this vertex
  void apply(icontext_type &context, vertex_type &vertex, const gather_type &total)
  {
    has_changed = false;

    for (SemiCluster c : total.clusters)
    {
      SemiCluster nC(c);
      nC.addToCluster(vertex.id(), total.neighbors);
      vertex.data().clusters.push_back(nC);
    }
  }

  edge_dir_type scatter_edges(icontext_type &context, const vertex_type &vertex) const
  {
    return graphlab::ALL_EDGES;
  }

  void scatter(icontext_type &context, const vertex_type &vertex, edge_type &edge) const
  {
    context.signal(edge.target());
  }
};

bool edge_list_parser(graph_type &graph, const std::string &filename, const std::string &line)
{
  if (line.empty())
    return true;

  int source, target;
  edge_data_type weight;

  if (sscanf(line.c_str(), "%i  %i  %f", &source, &target, &weight) < 3)
  {
    return false;
  }
  else
  {
    graph.add_edge(source, target, weight);
    return true;
  }
}

int main(int argc, char **argv)
{
  // Initialize control plain using mpi
  graphlab::mpi_tools::init(argc, argv);
  graphlab::distributed_control dc;
  global_logger().set_log_level(LOG_INFO);

  // Parse command line options -----------------------------------------------
  graphlab::command_line_options clopts("SemiClustering algorithm.");
  std::string graph_dir;
  std::string format = "adj";
  std::string exec_type = "synchronous";

  clopts.attach_option("graph", graph_dir,
                       "The graph file.  If none is provided "
                       "then a toy graph will be created");
  clopts.add_positional("graph");

  clopts.attach_option("c_max", C_MAX, "The c_max value");
  clopts.attach_option("v_max", V_MAX, "The v_max value");
  clopts.attach_option("f_b", F_B, "The fB value");

  clopts.attach_option("iterations", ITERATIONS, "How many iterations?");
  clopts.add_positional("iterations");

  std::string saveprefix;
  clopts.attach_option("saveprefix", saveprefix,
                       "If set, will save the resultant semiclusters to a "
                       "sequence of files with prefix saveprefix");

  if (!clopts.parse(argc, argv))
  {
    dc.cout() << "Error in parsing command line arguments." << std::endl;
    return EXIT_FAILURE;
  }

  if (ITERATIONS)
  {
    // make sure this is the synchronous engine
    dc.cout() << "--iterations set. Forcing Synchronous engine, and running "
              << "for " << ITERATIONS << " iterations." << std::endl;
    clopts.get_engine_args().set_option("type", "synchronous");
    clopts.get_engine_args().set_option("max_iterations", ITERATIONS);
    clopts.get_engine_args().set_option("sched_allv", true);
  }

  graph_type graph(dc, clopts);

  dc.cout() << "Loading graph in format: " << format << std::endl;
  graph.load(graph_dir, edge_list_parser);

  // must call finalize before querying the graph
  graph.finalize();

  dc.cout() << "#vertices: " << graph.num_vertices() << " #edges:" << graph.num_edges() << std::endl;

  // Initialize the vertex data
  graph.transform_vertices(init_vertex);

  // Running The Engine -------------------------------------------------------
  graphlab::omni_engine<semiclustering> engine(dc, graph, exec_type, clopts);
  engine.signal_all();
  engine.start();

  // MapReduce to gather result
  std::vector<SemiCluster> all = graph.map_reduce_vertices<cluster_reducer>(cluster_reducer::start).clusters;
  std::sort(all.begin(), all.end());
  std::reverse(all.begin(), all.end());

  all.resize(C_MAX);

  for (SemiCluster c : all)
  {
    dc.cout() << printCluster(c) << std::endl;
  }

  const double runtime = engine.elapsed_seconds();
  dc.cout() << "Finished Running engine in " << runtime
            << " seconds." << std::endl;

  // Tear-down communication layer and quit -----------------------------------
  graphlab::mpi_tools::finalize();
  return EXIT_SUCCESS;
}

// std::cout << "Vertex ID: " << vertex.id() << std::endl;
// std::cout << "Edge Source: " << edge.source().id() << std::endl;
// std::cout << "Edge Target: " << edge.target().id() << std::endl;
// std::cout << "W: " << edge.data() << std::endl;
// std::cout << "gather end" << std::endl;
