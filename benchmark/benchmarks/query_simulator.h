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
#include <time.h>
#include <iostream>
#include <fstream>
#include <unordered_map>

#include "llama/ll_writable_graph.h"
#include "benchmarks/benchmark.h"

using std::vector;
using std::deque;
using std::min;
using std::max;
using std::ifstream;
using std::unordered_map;

#define PARAM_ALPHA 0.5
#define PARAM_CACHE_SIZE 2000
#define PARAM_NUM_VERTICES 100000
#define PARAM_EPOCH_THRESHOLD 1
#define PARAM_FILENAME "temp.txt"

template <class T>
class lru_queue {
private:
    struct node {
        T value;
        unsigned int epoch;
        node* next;
        node* prev;
    };
    unordered_map<T, node*> map;
    unsigned int epoch;
    node* head;
    node* tail;
    omp_lock_t queue_lock;
public:
    lru_queue(): epoch(0), head(NULL), tail(NULL) {
        omp_init_lock(&queue_lock);
    }

    int enqueue(T x) {
	omp_set_lock(&queue_lock);
        typename unordered_map<T, node*>::iterator it;
        if ((it = map.find(x)) != map.end()) {
            node* pointer = it->second;
            node* old_next = pointer->next;
	    if (pointer != head) {
		    pointer->next = head;
		    head->prev = pointer;
		    pointer->prev->next = old_next;
		    if (old_next) {
			    old_next->prev = pointer->prev;
		    }
		    pointer->prev = NULL;
		    pointer->epoch = epoch;
		    head = pointer;
	    }
        } else {
            node* pointer = (node*)malloc(sizeof(node));
            if (pointer == NULL) {
		    omp_unset_lock(&queue_lock);
		    return 1;
	    }
            pointer->value = x;
            pointer->epoch = epoch;
            pointer->next = head;
	    if (head) {
		    head->prev = pointer;
	    }
            pointer->prev = NULL;
            head = pointer;
            map[x] = pointer;
        }

	omp_unset_lock(&queue_lock);
	return 0;
    }

    void increment_epoch() {
        epoch++;
    }

    unsigned int get_current_epoch() {
        return epoch;
    }

    int dequeue(T* ret, unsigned int* epoch_ret) {
	omp_set_lock(&queue_lock);
        if (head == NULL) {
		omp_unset_lock(&queue_lock);
		return 1;
	}
	if (epoch - head->epoch > 500) {
		omp_unset_lock(&queue_lock);
		return 1;
	}

        map.erase(head->value);
	
        *ret = head->value;
        *epoch_ret = head->epoch;
        node* newhead = head->next;
        free(head);
        head = newhead;
	if (newhead) {
		newhead->prev = NULL;
	}

	omp_unset_lock(&queue_lock);
	return 0;
    }
};

/**
 * Count vertices in the given friends of friends traversal
 */
template <class Graph>
class ll_b_query_simulator : public ll_benchmark<Graph> {

	node_t root;
	node_t requests[PARAM_NUM_VERTICES];

public:

	/**
	 * Create the benchmark
	 *
	 * @param graph the graph
	 * @param r the root
	 */
	ll_b_query_simulator(Graph& graph)
		: ll_benchmark<Graph>(graph, "Query simulator") {
	}

	/**
	 * Destroy the benchmark
	 */
	virtual ~ll_b_query_simulator(void) {
	}

	virtual void initialize(void) {
	    ifstream input_file;
	    input_file.open(PARAM_FILENAME);
	    for (int i = 0; i < PARAM_NUM_VERTICES; i++)
                input_file >> requests[i];
	}

	/**
	 * Run the benchmark
	 *
	 * @return the numerical result, if applicable
	 */
	virtual double run(void) {

	    Graph& G = this->_graph;
	    // float avg = 0;
	    node_t sum = 0; // We don't want compiler to optimize away loops

	    ll_edge_iterator iterm;
#ifdef LL_BM_DO_MADVISE
	    G.out_iter_do_madvise(iterm);
#endif
	    // Query the graph
	    for (int i = 0; i < PARAM_NUM_VERTICES; ++i) {
		    node_t n = requests[i];
                    G.out_iter_begin(iterm, n);
                    FOREACH_OUTEDGE_ITER(v_idx, G, iterm) {
                           node_t next_node = iterm.last_node;
                           sum += next_node; // Don't optimize this out
	            }
            }

	    G.out_iter_stop_madvise(iterm);
            
            return sum; 
    }

};

#endif // #ifndef LL_FRIEND_OF_FRIENDS_H
