/*
 * query_creator.cc
 * LLAMA Graph Analytics
 *
 * Copyright 2014
 *      The President and Fellows of Harvard College.
 *
 * Copyright 2014
 *      Oracle Labs.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * Query creator
 */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <getopt.h>
#include <libgen.h>
#include <fstream>

#include <llama.h>

using std::ofstream;


template <class Graph>
class request_generator {

	Graph& graph;
	deque<node_t> cache;
	double alpha;
	unsigned int cache_size;
    ofstream friends_file;
public:
	request_generator(Graph& graph, double alpha=0.1, unsigned int cache_size=200)
		: graph(graph), alpha(alpha), cache_size(cache_size) {
		srand(time(NULL));
		friends_file.open("friends_list.txt");
	}

	node_t generate() {
		Graph& G = this->graph;
		int rand_num = 0;
		node_t retval;
		if (!cache.empty()) {
			// choose random number
			rand_num = rand();
		}

		if (rand_num < RAND_MAX * alpha) {
			// choose uniformly over all graph nodes
			retval = G.pick_random_node();
		} else {
			int rand_cache = rand() % cache.size();
			retval = cache[rand_cache];
		}

		unsigned int i = 0;
		while (i++ < cache_size/5 && cache.size() > cache_size) {
			cache.pop_back();
		}

		cache.push_front(retval);
		ll_edge_iterator iterm;
		G.out_iter_begin(iterm, retval);
		unsigned int num_inserted = 0;
		FOREACH_OUTEDGE_ITER(v_idx, G, iterm) {
		    node_t next_node = iterm.last_node;
		    friends_file <<  next_node << " ";
		    if (num_inserted > cache_size/5) continue;
		    int rand_insert = rand() % 4;
		    if (rand_insert == 0) {
			    num_inserted++;
			    cache.push_front(next_node);
		    }
		}
		friends_file << "\n";
		return retval;
	}
};
//==========================================================================//
// The Command-Line Arguments                                               //
//==========================================================================//

static const char* SHORT_OPTIONS = "d:ht:va:n:";

static struct option LONG_OPTIONS[] =
{
	{"alpha"        , required_argument, 0, 'a'},
	{"database"     , required_argument, 0, 'd'},
	{"help"         , no_argument,       0, 'h'},
	{"query_number" , required_argument, 0, 'n'},
	{"threads"      , required_argument, 0, 't'},
	{0, 0, 0, 0}
};


/**
 * Print the usage information
 *
 * @param arg0 the first element in the argv array
 */
static void usage(const char* arg0) {

	char* s = strdup(arg0);
	char* p = basename(s);
	fprintf(stderr, "Usage: %s [OPTIONS]\n\n", p);
	free(s);
	
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -a, --alpha f         Value of alpha\n");
	fprintf(stderr, "  -d, --database DIR    Set the database directory\n");
	fprintf(stderr, "  -h, --help            Show this usage information and exit\n");
	fprintf(stderr, "  -n, --query_number N  Number of queries to generate\n");
	fprintf(stderr, "  -t, --threads N       Set the number of threads\n");
}


//==========================================================================//
// The Main Function                                                        //
//==========================================================================//

/**
 * The main function
 */
int main(int argc, char** argv)
{
	char* database_directory = NULL;
	int num_threads = -1;
  size_t N = 10000;


	// Pase the command-line arguments

	int option_index = 0;
  float alpha = 0.5;
	while (true) {
		int c = getopt_long(argc, argv, SHORT_OPTIONS, LONG_OPTIONS, &option_index);

		if (c == -1) break;

		switch (c) {
      case 'a':
        alpha = atof(optarg);
        break;

			case 'd':
				database_directory = optarg;
				break;

			case 'h':
				usage(argv[0]);
				return 0;

			case 't':
				num_threads = atoi(optarg);
				break;

      case 'n':
        N = atoi(optarg);
        break;

			case '?':
			case ':':
				return 1;

			default:
				abort();
		}
	}

	if (optind != argc) {
		fprintf(stderr, "Error: Too many command line arguments\n");
		return 1;
	}


	// Open the database
	
	ll_database database(database_directory);
	if (num_threads > 0) database.set_num_threads(num_threads);
	ll_mlcsr_ro_graph& graph = database.graph()->ro_graph();

  request_generator<ll_mlcsr_ro_graph> generator(graph, alpha, 2000);
  ofstream output_file;
  output_file.open("temp.txt");
  fprintf(stderr, "Generating %d queries with alpha=%.2f\n", N, alpha);
  // Query the graph
  for (size_t i = 0; i < N; ++i) {
                node_t n = generator.generate();
    output_file << n << "\n"; 
    }

            return 0;    


	return 0;
}
