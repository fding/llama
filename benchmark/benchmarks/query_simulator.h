/*
 * query_simulator.h
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


#ifndef LL_QUERY_SIMULATOR_H
#define LL_QUERY_SIMULATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <float.h>
#include <limits.h>
#include <cmath>
#include <algorithm>
#include <omp.h>
#include <vector>

#include "llama/ll_writable_graph.h"
#include "benchmarks/benchmark.h"

// TODO(fding): make runtime flag, etc
// #define DO_MADVISE
// #define DO_MUNADVISE

using std::vector;
using std::deque;

template <class Graph>
class request_generator {

	Graph& graph;
	deque<node_t> cache;
	double alpha;
	unsigned int cache_size;
public:
	request_generator(Graph& graph, double alpha=0.5, unsigned int cache_size=200)
		: graph(graph), alpha(alpha), cache_size(cache_size) {
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
class ll_b_query_simulator : public ll_benchmark<Graph> {

	node_t root;
	request_generator<Graph> generator;


public:

	/**
	 * Create the benchmark
	 *
	 * @param graph the graph
	 * @param r the root
	 */
	ll_b_query_simulator(Graph& graph)
		: ll_benchmark<Graph>(graph, "Query simulator"), generator(graph, 0.5, 2000) {
	}


	/**
	 * Destroy the benchmark
	 */
	virtual ~ll_b_query_simulator(void) {
	}


	/**
	 * Run the benchmark
	 *
	 * @return the numerical result, if applicable
	 */
	virtual double run(void) {

	    Graph& G = this->_graph;
	    // float avg = 0;
	    int num_vertices = 1000;

	    // Query the graph
	    for (int i = 0; i < num_vertices; ++i) {
		node_t n = this->generator.generate();
		ll_edge_iterator iterm;
		G.out_iter_begin(iterm, n);
#ifdef DO_MADVISE
		auto vtable = G.out().vertex_table(0);
		auto etable = G.out().edge_table(0);
#endif
		FOREACH_OUTEDGE_ITER(v_idx, G, iterm) {
			node_t next_node = iterm.last_node;
			(void) next_node; // Don't optimize this out
#ifdef DO_MADVISE
			edge_t first = (*vtable)[next_node].adj_list_start;
		        edge_t last = first + (*vtable)[next_node].level_length;
			if (last - first > 0)
			    etable->advise(first, last);
#endif
		}
            }
	    return 0; 
	}
};

#undef DO_MADVISE
#endif // #ifndef LL_FRIEND_OF_FRIENDS_H
