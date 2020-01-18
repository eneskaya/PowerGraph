/*  
 * Copyright (c) 2009 Carnegie Mellon University. 
 *     All rights reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing,
 *  software distributed under the License is distributed on an "AS
 *  IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 *  express or implied.  See the License for the specific language
 *  governing permissions and limitations under the License.
 *
 *
 */


#include <graphlab.hpp>

// The vertex data is just the pagerank value (a float)
typedef std::pair<int, int> vertex_data_type;

// There is no edge data in the pagerank application
typedef graphlab::empty edge_data_type;

// The graph type is determined by the vertex and edge data types
typedef graphlab::distributed_graph<vertex_data_type, edge_data_type> graph_type;

/*
 * A simple function used by graph.transform_vertices(init_vertex);
 * to initialize the vertex data.
 */
void init_vertex(graph_type::vertex_type& vertex) {
    std::pair<int, int> e(vertex.num_in_edges(), vertex.num_out_edges());
    vertex.data() = e;
}

struct concomp_writer {
    std::string save_vertex(graph_type::vertex_type v) {
        std::stringstream strm;
        strm << v.id() << "," << v.data().first << "," << v.data().second << "\n";
        return strm.str();
    }
    std::string save_edge(graph_type::edge_type e) { return ""; }
};

int main(int argc, char** argv) {
   // Initialize control plain using mpi
   graphlab::mpi_tools::init(argc, argv);
   graphlab::distributed_control dc;
   global_logger().set_log_level(LOG_INFO);
    
   // Parse command line options -----------------------------------------------
   graphlab::command_line_options clopts("PageRank algorithm.");
   std::string graph_dir;
   std::string save_prefix;
   std::string format = "snap";
   
   clopts.attach_option("graph", graph_dir, "The graph file. Required ");
   clopts.add_positional("graph");
   clopts.attach_option("save", save_prefix, "File name to save");
   clopts.add_positional("save");
   clopts.attach_option("format", format, "The graph file format");
   
   if(!clopts.parse(argc, argv)) {
       dc.cout() << "Error in parsing command line arguments." << std::endl;
       return EXIT_FAILURE;
   }
   if (graph_dir == "") {
       dc.cout() << "Graph not specified. Cannot continue";
       return EXIT_FAILURE;
   }
   
   // Build the graph ----------------------------------------------------------
   graph_type graph(dc, clopts);
   dc.cout() << "Loading graph in format: "<< format << std::endl;
   graph.load_format(graph_dir, format);
   // must call finalize before querying the graph
   graph.finalize();

   graph.transform_vertices(init_vertex);

   dc.cout() << "#vertices: " << graph.num_vertices() << " #edges:" << graph.num_edges() << std::endl;

//    graphlab::omni_engine<concomp> engine(dc, graph, "synchronous", clopts);
//    engine.signal_all();

//    engine.start();

//    const float runtime = engine.elapsed_seconds();
//    dc.cout() << "Finished Running engine in " << runtime << " seconds." << std::endl;

   graph.save(save_prefix, concomp_writer(),
               false,    // do not gzip
               true,     // save vertices
               false);   // do not save edges
}
