/*
 * shortest_path.h
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


#ifndef LL_SHORTEST_PATH_H
#define LL_SHORTEST_PATH_H

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

#define PARAM_SHORTEST_N 1000
#define PARAM_FILENAME "temp.txt"

using std::vector;
using std::ifstream;

/**
 */
template <class Graph>
class ll_b_shortest_path : public ll_benchmark<Graph> {

	node_t root;
	node_t requests[PARAM_SHORTEST_N];
  int* dist[2];

public:

	/**
	 * Create the benchmark
	 *
	 * @param graph the graph
	 * @param r the root
	 */
	ll_b_shortest_path(Graph& graph)
		: ll_benchmark<Graph>(graph, "Shortest path") {
		srand(time(NULL));
    dist[0] = (int*) malloc(graph.max_nodes()*sizeof(int));
    dist[1] = (int*) malloc(graph.max_nodes()*sizeof(int));
	}


	/**
	 * Destroy the benchmark
	 */
	virtual ~ll_b_shortest_path(void) {
	}

	virtual void initialize(void) {
	    ifstream input_file;
	    input_file.open(PARAM_FILENAME);
	    for (int i = 0; i < PARAM_SHORTEST_N; i++)
                input_file >> requests[i];
	}

	/**
	 * Run the benchmark
	 *
	 * @return the numerical result, if applicable
	 */
	virtual double run(void) {

	    Graph& G = this->_graph;
	    float avg = 0;

#ifdef LL_BM_DO_MADVISE
	    ll_advisor<Graph> advisor(&G);
#endif
	    for (int i = 0; i < PARAM_SHORTEST_N-1; i += 2) {
        node_t first;
        node_t second;
        first = requests[i];
        second = requests[i+1];
#pragma omp parallel for
        for (node_t t0 = 0; t0 < G.max_nodes(); t0++) {
          dist[0][t0] = -1;
          dist[1][t0] = -1;
        }

        deque<node_t> queues[2];
        dist[0][first] = 0;
        dist[1][second] = 0;
        queues[0].push_back(first);
        queues[1].push_back(second);
        bool found = false;
        unsigned int distance = UINT_MAX;

#pragma omp parallel for
        for (int j = 0; j < 2; j++) {
          while (!queues[j].empty() && !found) {
            node_t node = queues[j].front();
            queues[j].pop_front();
            ll_edge_iterator it;
            edge_t e;
            if (j == 0) {
              G.out_iter_begin(it, node);
              e = G.out_iter_next(it);
#ifdef LL_BM_DO_MADVISE
              advisor.advise(node);
#endif
            }
            else {
              G.in_iter_begin(it, node);
              e = G.in_iter_next(it);
            }

            for (; e != LL_NIL_EDGE; ) {
              if (found) break;
              node_t next = it.last_node;
              if (dist[j][next] != -1) {
                if (j == 0) {
                  e = G.out_iter_next(it);
                }
                else {
                  e = G.in_iter_next(it);
                }
                continue;
              }
#pragma omp critical
              {
                if (dist[1-j][next] != -1) {
                  if (!found) {
                    found = true;
                    distance = dist[1-j][next] + dist[j][next];
                  }
                }
                dist[j][next] = dist[j][node]+1;
              }
              queues[j].push_back(next);
              if (j == 0) {
                e = G.out_iter_next(it);
              }
              else {
                e = G.in_iter_next(it);
              }
            }
          }
        }
        avg += distance;
      }

#ifdef LL_BM_DO_MADVISE
	    advisor.stop();
#endif

	    return avg / PARAM_SHORTEST_N; 
	}
};

#endif // #ifndef LL_SHORTEST_PATH_H
