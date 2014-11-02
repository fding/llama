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

#include "llama/ll_writable_graph.h"
#include "benchmarks/benchmark.h"

using std::vector;
using std::deque;

/* A queue implementation for producer-consumer problems.
 * All enqueues are assumed to happen in one thread,
 * and all dequeues in another.
 * Enqueues and dequeues can occur in different threads.
 * Data structure does not need to acquire locks to guarantee
 * synchronization in this situation. */
template <class T, int BLOCKSIZE=16384-32>
class syncqueue {
    // Not particularly memory efficient, but make implementation really simple and easy to reason about
    private:
    struct node {
        T value;
        unsigned int epoch;
        node* next;
    };
    node* head;
    node* head_section;
    int tail;
    node* tail_section;
    // To avoid race conditions when freeing after dequeue
    node* tmp_head_section;
    unsigned int current_epoch;

    public:

        // We set tail=BLOCKSIZE-1 to guarantee that in the first enqueue,
        // tail_section is malloced.
        syncqueue():
            head(NULL), head_section(NULL), tail(BLOCKSIZE-1), tail_section(NULL),
            tmp_head_section(NULL), current_epoch(0)
        {}

        ~syncqueue(){
            // TODO(fding): This code is completely bogus.
            // It actually doesn't free up all the memory. Fix! (we'd need to walk the linked list)
            //
            // TODO(fding): It would be helpful to add logging information here,
            // to see how fast dequeuing occurs relative to enqueuing.
            if (tmp_head_section) {
                free(tmp_head_section);
            }
        }

        int enqueue(T x) {
            int i;
            if (tail == BLOCKSIZE - 1) {
                tail_section = (node*) malloc(BLOCKSIZE * sizeof(node));
                if (!tail_section) return 1;
                i = 0;
            }
            else i = tail + 1;
            // We fully initialize the new node before linking it in.
            tail_section[i].value = x;
            tail_section[i].epoch = current_epoch;
            tail_section[i].next = NULL;
            node* old = &tail_section[tail];
            tail = i;
            if (old) old->next = &tail_section[tail];
            if (head == NULL) {
                head_section = tail_section;
                head = &tail_section[tail];
            }
            return 0;
        }

        void increment_epoch() {
            current_epoch++;
        }

        unsigned int get_current_epoch() {
            return current_epoch;
        }

        int dequeue(T* ret, unsigned int * epoch) {
            if (head == NULL) return 1;
            *ret = head->value;
            *epoch = head->epoch;
            if (tmp_head_section) {
                free(tmp_head_section);
                tmp_head_section = NULL;
            }
            // Instead of freeing head_section (which could equal tail_section),
            // we record that we need to free it.
            if (head-head_section == BLOCKSIZE - 1) {
                tmp_head_section = head_section;
                head_section = head->next;
            }
            head = head->next;
            return 0;
        }
};



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
        node_t sum = 0; // We don't want compiler to optimize away loops

#ifdef LL_BM_DO_MADVISE
        // Page-size, minus malloc overhead.
        syncqueue<node_t> nodes_to_advise;
	    bool still_adding = true;

#pragma omp parallel sections
{
    #pragma omp section
    {
	    while (still_adding) {
		    node_t add;
		    unsigned int epoch;
		    if (nodes_to_advise.dequeue(&add, &epoch)) continue;
		    if (nodes_to_advise.get_current_epoch() - epoch > 2) continue;

		    ll_edge_iterator iteradd;
		    G.out_iter_begin(iteradd, add);

		    auto vtable = G.out().vertex_table(0);
		    auto etable = G.out().edge_table(0);
		    edge_t first = (*vtable)[add].adj_list_start;
		    edge_t last = first + (*vtable)[add].level_length;
		    if (last - first > 0) etable->advise(first, last);

		    /*FOREACH_OUTEDGE_ITER(v_idx, G, iteradd) {
			    node_t next_node = iteradd.last_node;
			    edge_t first = (*vtable)[next_node].adj_list_start;
			    edge_t last = first + (*vtable)[next_node].level_length;
			    if (last - first > 0) etable->advise(first, last);
		    }*/
	    }
    }  // end #pragma omp section
    #pragma omp section
    {
#endif
	    // Query the graph
	    for (int i = 0; i < num_vertices; ++i) {
            node_t n = this->generator.generate();
            ll_edge_iterator iterm;
            G.out_iter_begin(iterm, n);
#ifdef LL_BM_DO_MADVISE
            nodes_to_advise.increment_epoch();
            nodes_to_advise.enqueue(n);
#endif
            FOREACH_OUTEDGE_ITER(v_idx, G, iterm) {
                node_t next_node = iterm.last_node;
                sum += next_node; // Don't optimize this out
	    }
        }
            
#ifdef LL_BM_DO_MADVISE
	    still_adding = false;
    }  // end #pragma omp section
}  // end #pragma omp sections
#endif
    return sum; 
    }

};

#endif // #ifndef LL_FRIEND_OF_FRIENDS_H
