/*
 * friend_of_friends.h
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


#ifndef LL_FRIEND_OF_FRIENDS_H
#define LL_FRIEND_OF_FRIENDS_H

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

#include "llama/ll_writable_graph.h"
#include "benchmarks/benchmark.h"

// TODO(fding): make runtime flag, etc
// #define DO_MADVISE
// #define DO_MUNADVISE

using std::vector;

/**
 * Count vertices in the given friends of friends traversal
 */
template <class Graph>
class ll_b_friend_of_friends : public ll_benchmark<Graph> {

	node_t root;


public:

	/**
	 * Create the benchmark
	 *
	 * @param graph the graph
	 * @param r the root
	 */
	ll_b_friend_of_friends(Graph& graph)
		: ll_benchmark<Graph>(graph, "Friend of friends") {
		srand(time(NULL));
	}


	/**
	 * Destroy the benchmark
	 */
	virtual ~ll_b_friend_of_friends(void) {
	}


	/**
	 * Run the benchmark
	 *
	 * @return the numerical result, if applicable
	 */
	virtual double run(void) {

	    Graph& G = this->_graph;
	    float avg = 0;
	    int num_vertices = 50;

	    for (int i = 0; i < num_vertices; ++i) {
		node_t n = G.pick_random_node();
#ifdef DO_MADVISE
		int done = 0;
#pragma omp parallel sections
{
    #pragma omp section
    {
		ll_edge_iterator iterm;
		G.out_iter_begin(iterm, n);
		auto vtable = G.out().vertex_table(0);
		auto etable = G.out().edge_table(0);
		int j = 0;
		FOREACH_OUTEDGE_ITER(v_idx, G, iterm) {
		    if (j <= done) {
			j++;
			continue;
		    }
		    node_t next_node = iterm.last_node;
		    edge_t first = (*vtable)[next_node].adj_list_start;
		    edge_t last = first + (*vtable)[next_node].level_length;
		    if (last - first > 0)
                        etable->advise(first, last);
		    j++;
		}
    }
    #pragma omp section
    { // Friend of friends iteration
#endif
		ll_edge_iterator iter;
		vector<node_t> results;
		G.out_iter_begin(iter, n);
		FOREACH_OUTEDGE_ITER(v_idx, G, iter) {
			node_t n2 = LL_ITER_OUT_NEXT_NODE(G, iter, v_idx);
			
			ll_edge_iterator iter2;
			G.out_iter_begin(iter2, n2);
#ifdef DO_MADVISE
			done++;
#endif
			FOREACH_OUTEDGE_ITER(v2_idx, G, iter2) {
				node_t n3 = LL_ITER_OUT_NEXT_NODE(G, iter2, v2_idx);
				results.push_back(n3);
			}
		}
		avg += results.size();
		results.clear();
#ifdef DO_MADVISE
    } // END friend of friends section
} // #pragma omp sections
#endif
	    }

	    return avg / num_vertices; 
	}
};

#undef DO_MADVISE
#endif // #ifndef LL_FRIEND_OF_FRIENDS_H
