#include <graphlab.hpp>

typedef graphlab::empty vertex_data_type;

// float weight
typedef float edge_data_type;

// The graph type is determined by the vertex and edge data types
typedef graphlab::distributed_graph<vertex_data_type, edge_data_type> graph_type;

void init_vertex(graph_type::vertex_type& vertex) {}

void init_edge(graph_type::edge_type& edge) {
	edge.data() = (rand() / (float)RAND_MAX * 9) + 1;
}

struct concomp_writer {
    std::string save_vertex(graph_type::vertex_type v) {
        return "";
    }

    std::string save_edge(graph_type::edge_type e) {
		std::stringstream strm;
		strm << e.source().id() << "\t" << e.target().id() << "\t" << e.data() << "\n";
		return strm.str();
	}
};

int main(int argc, char** argv) {
  // Initialize control plain using mpi
  graphlab::mpi_tools::init(argc, argv);
  graphlab::distributed_control dc;
  global_logger().set_log_level(LOG_INFO);

  // Parse command line options -----------------------------------------------
  graphlab::command_line_options clopts("GraphGen");
  std::string save_prefix;
  size_t powerlaw = 0;

  clopts.attach_option("save", save_prefix, "File name to save");
  clopts.add_positional("save");

  clopts.attach_option("powerlaw", powerlaw, "Generate a synthetic powerlaw out-degree graph. ");

  if (!clopts.parse(argc, argv))
  {
    dc.cout() << "Error in parsing command line arguments." << std::endl;
    return EXIT_FAILURE;
  }

  graph_type graph(dc, clopts);

  dc.cout() << "Loading synthetic Powerlaw graph." << powerlaw << std::endl;
  graph.load_synthetic_powerlaw(powerlaw, true, 2.1, 100000000);
      
  // Build the graph ----------------------------------------------------------
  // must call finalize before querying the graph
  graph.finalize();
  // Generate random weights for the edges
  graph.transform_edges(init_edge);

  dc.cout() << "#vertices: " << graph.num_vertices() << " #edges:" << graph.num_edges() << std::endl;

  graph.save(save_prefix, concomp_writer(),
              false,	// do not gzip
              false,   // save vertices
              true);   // save edges
}
