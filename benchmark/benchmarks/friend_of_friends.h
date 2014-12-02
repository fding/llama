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
#include <iostream>
#include <fstream>

#include "llama/ll_writable_graph.h"
#include "llama/ll_mlcsr_graph.h"
#include "llama/ll_advisor.h"
#include "benchmarks/benchmark.h"

#define PARAM_N 1000000
#define PARAM_FILENAME "temp.txt"

using std::vector;
using std::ifstream;

/**
 * Count vertices in the given friends of friends traversal
 */
template <class Graph>
class ll_b_friend_of_friends : public ll_benchmark<Graph> {

	node_t root;
	node_t requests[PARAM_N];
	size_t num_vertices;

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

	virtual void initialize(void) {
	    Graph& G = this->_graph;

	    ifstream input_file;
	    input_file.open(PARAM_FILENAME);
		while (input_file >> requests[num_vertices++]);
		num_vertices --;
		size_t warmup_n = num_vertices > 200000 ? 200000 : num_vertices;

		struct rusage r_start, r_end;
		getrusage(RUSAGE_SELF, &r_start);
		double t_start = ll_get_time_ms();

#ifdef LL_BM_DO_MADVISE
	    ll_advisor<Graph> advisor(&G);
#endif
	    for (int i = 0; i < warmup_n; ++i) {
		node_t n = requests[i];
		ll_edge_iterator iter;
		vector<node_t> results;
		G.out_iter_begin(iter, n);
#ifdef LL_BM_DO_MADVISE
		advisor.advise(n);
#endif
		FOREACH_OUTEDGE_ITER(v_idx, G, iter) {
			node_t n2 = LL_ITER_OUT_NEXT_NODE(G, iter, v_idx);
			
			ll_edge_iterator iter2;
			G.out_iter_begin(iter2, n2);
			FOREACH_OUTEDGE_ITER(v2_idx, G, iter2) {
				node_t n3 = LL_ITER_OUT_NEXT_NODE(G, iter2, v2_idx);
				results.push_back(n3);
			}
		}
		results.clear();
	    }
#ifdef LL_BM_DO_MADVISE
	    advisor.stop();
#endif
		double t_end = ll_get_time_ms();
		getrusage(RUSAGE_SELF, &r_end);
		printf("Warmup Time      : %.3f\n", t_end-t_start);
		printf("Warmup User Time : %.3f\n", ll_timeval_to_ms(r_end.ru_utime) - ll_timeval_to_ms(r_start.ru_utime));
		printf("Warmup System Time : %.3f\n", ll_timeval_to_ms(r_end.ru_stime) - ll_timeval_to_ms(r_start.ru_stime));
		printf("Warmup Cpu Time : %.3f\n", ll_timeval_to_ms(r_end.ru_stime) + ll_timeval_to_ms(r_end.ru_utime) - ll_timeval_to_ms(r_start.ru_stime) - ll_timeval_to_ms(r_start.ru_utime));
	}

	/**
	 * Run the benchmark
	 *
	 * @return the numerical result, if applicable
	 */
	virtual double run(void) {

	    Graph& G = this->_graph;
	    float avg = 0;
		size_t warmup_n = num_vertices > 200000 ? 200000 : num_vertices;

#ifdef LL_BM_DO_MADVISE
	    ll_advisor<Graph> advisor(&G);
#endif
	    for (int i = warmup_n; i < num_vertices; ++i) {
		node_t n = requests[i];
		ll_edge_iterator iter;
		vector<node_t> results;
		G.out_iter_begin(iter, n);
#ifdef LL_BM_DO_MADVISE
		advisor.advise(n);
#endif
		FOREACH_OUTEDGE_ITER(v_idx, G, iter) {
			node_t n2 = LL_ITER_OUT_NEXT_NODE(G, iter, v_idx);
			
			ll_edge_iterator iter2;
			G.out_iter_begin(iter2, n2);
			FOREACH_OUTEDGE_ITER(v2_idx, G, iter2) {
				node_t n3 = LL_ITER_OUT_NEXT_NODE(G, iter2, v2_idx);
				results.push_back(n3);
			}
		}
		avg += results.size();
		results.clear();
	    }
#ifdef LL_BM_DO_MADVISE
	    advisor.stop();
#endif

	    return avg / PARAM_N; 
	}
};

#endif // #ifndef LL_FRIEND_OF_FRIENDS_H
