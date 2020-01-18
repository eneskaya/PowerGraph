#include <vector>
#include <string>
#include <fstream>

#include <graphlab.hpp>

double C_MAX = 15;
double V_MAX = 5;
double F_B = 0.1;

// The vertex data is just the pagerank value (a double)
typedef semi_cluster_container vertex_data_type;

// double for weight as edge data
typedef double edge_data_type;

// The graph type is determined by the vertex and edge data types
typedef graphlab::distributed_graph<vertex_data_type, edge_data_type> graph_type;

struct semi_cluster_container
{
  std::vector<std::pair<double, std::vector<graphlab::vertex_id_type>>> cluster_list;

  void save(graphlab::oarchive &oarc) const
  {
    oarc << cluster_list;
  }

  void load(graphlab::iarchive &iarc)
  {
    iarc >> cluster_list;
  }

};

/*
 * A simple function used by graph.transform_vertices(init_vertex);
 * to initialize the vertes data.
 */
void init_vertex(graph_type::vertex_type &vertex)
{
  vertex.data();
}

class pagerank : public graphlab::ivertex_program<graph_type, std::vector<semi_cluster>>
{

  double last_change;

public:
  /**
   * Gather all edges.
   */
  edge_dir_type gather_edges(icontext_type &context,
                             const vertex_type &vertex) const
  {
    return graphlab::ALL_EDGES;
  }

  /* Gather the weighted rank of the adjacent page   */
  double gather(icontext_type &context, const vertex_type &vertex,
                edge_type &edge) const
  {
  }

  /* Use the total rank of adjacent pages to update this page */
  void apply(icontext_type &context, vertex_type &vertex,
             const gather_type &total)
  {
  }

  /* The scatter edges depend on whether the pagerank has converged */
  edge_dir_type scatter_edges(icontext_type &context,
                              const vertex_type &vertex) const
  {
  }

  /* The scatter function just signal adjacent pages */
  void scatter(icontext_type &context, const vertex_type &vertex,
               edge_type &edge) const
  {
  }

  void save(graphlab::oarchive &oarc) const
  {
  }

  void load(graphlab::iarchive &iarc)
  {
  }
};

int main(int argc, char **argv)
{
  // Initialize control plain using mpi
  graphlab::mpi_tools::init(argc, argv);
  graphlab::distributed_control dc;
  global_logger().set_log_level(LOG_INFO);

  // Parse command line options -----------------------------------------------
  graphlab::command_line_options clopts("PageRank algorithm.");
  std::string graph_dir;
  std::string format = "adj";
  std::string exec_type = "synchronous";
  clopts.attach_option("graph", graph_dir,
                       "The graph file.  If none is provided "
                       "then a toy graph will be created");
  clopts.add_positional("graph");
  clopts.attach_option("engine", exec_type,
                       "The engine type synchronous or asynchronous");
  clopts.attach_option("format", format,
                       "The graph file format");

  size_t powerlaw = 0;

  clopts.attach_option("powerlaw", powerlaw,
                       "Generate a synthetic powerlaw out-degree graph. ");

  clopts.attach_option("c_max", C_MAX, "");
  clopts.attach_option("v_max", V_MAX, "");
  clopts.attach_option("f_b", F_B, "");

  std::string saveprefix;
  clopts.attach_option("saveprefix", saveprefix,
                       "If set, will save the resultant pagerank to a "
                       "sequence of files with prefix saveprefix");

  if (!clopts.parse(argc, argv))
  {
    dc.cout() << "Error in parsing command line arguments." << std::endl;
    return EXIT_FAILURE;
  }

  // Enable gather caching in the engine
  // clopts.get_engine_args().set_option("use_cache", USE_DELTA_CACHE);

  // Build the graph ----------------------------------------------------------
  graph_type graph(dc, clopts);
  if (powerlaw > 0)
  { // make a synthetic graph
    dc.cout() << "Loading synthetic Powerlaw graph." << std::endl;
    graph.load_synthetic_powerlaw(powerlaw, false, 2.1, 100000000);
  }
  else if (graph_dir.length() > 0)
  { // Load the graph from a file
    dc.cout() << "Loading graph in format: " << format << std::endl;
    graph.load_format(graph_dir, format);
  }
  else
  {
    dc.cout() << "graph or powerlaw option must be specified" << std::endl;
    clopts.print_description();
    return 0;
  }
  // must call finalize before querying the graph
  graph.finalize();
  dc.cout() << "#vertices: " << graph.num_vertices()
            << " #edges:" << graph.num_edges() << std::endl;

  // Initialize the vertex data
  graph.transform_vertices(init_vertex);

  // Running The Engine -------------------------------------------------------
  graphlab::omni_engine<pagerank> engine(dc, graph, exec_type, clopts);
  engine.signal_all();
  engine.start();
  const double runtime = engine.elapsed_seconds();
  dc.cout() << "Finished Running engine in " << runtime
            << " seconds." << std::endl;

  // Tear-down communication layer and quit -----------------------------------
  graphlab::mpi_tools::finalize();
  return EXIT_SUCCESS;
} // End of main

// We render this entire program in the documentation
