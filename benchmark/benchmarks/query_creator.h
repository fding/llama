/*
 * query_creator.h
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


#ifndef LL_QUERY_CREATOR_H
#define LL_QUERY_CREATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <float.h>
#include <limits.h>
#include <cmath>
#include <algorithm>
#include <omp.h>
#include <vector>
#include <time.h>
#include <iostream>
#include <fstream>

#include "llama/ll_writable_graph.h"
#include "benchmarks/benchmark.h"

using std::vector;
using std::deque;
using std::min;
using std::max;
using std::ofstream;

#define ALPHA 0.5
#define CACHE_SIZE 2000
#define NUM_VERTICES 100000
#define FILENAME "temp.txt"

template <class Graph>
class request_generator {

	Graph& graph;
	deque<node_t> cache;
	double alpha;
	unsigned int cache_size;
public:
	request_generator(Graph& graph, double alpha=0.1, unsigned int cache_size=200)
		: graph(graph), alpha(alpha), cache_size(cache_size) {
		srand(time(NULL));
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
		    int rand_insert = rand() % 4;
		    if (rand_insert == 0) {
			    num_inserted++;
			    cache.push_front(next_node);
		    }
		    if (num_inserted > cache_size/5) break;
		}
		return retval;
	}
};

/**
 * Count vertices in the given friends of friends traversal
 */
template <class Graph>
class ll_b_query_creator : public ll_benchmark<Graph> {

	node_t root;
	request_generator<Graph> generator;

public:

	/**
	 * Create the benchmark
	 *
	 * @param graph the graph
	 * @param r the root
	 */
	ll_b_query_creator(Graph& graph)
		: ll_benchmark<Graph>(graph, "Query simulator"), generator(graph, ALPHA, CACHE_SIZE) {
	}

	/**
	 * Destroy the benchmark
	 */
	virtual ~ll_b_query_creator(void) {
	}

	/**
	 * Run the benchmark
	 *
	 * @return the numerical result, if applicable
	 */
	virtual double run(void) {

	    Graph& G = this->_graph;
	    ofstream output_file;
	    output_file.open(FILENAME);
	    // Query the graph
	    for (int i = 0; i < NUM_VERTICES; ++i) {
                    node_t n = this->generator.generate();
		    output_file << n << "\n"; 
            }

            return 0;    
	}
};

#endif
