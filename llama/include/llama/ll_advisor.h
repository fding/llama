/*
 * ll_advisor_.h
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


#ifndef LL_ADVISOR_H_
#define LL_ADVISOR_H_

#include "llama/ll_mem_array.h"
#include "llama/ll_writable_elements.h"

#include "llama/ll_mlcsr_helpers.h"
#include "llama/ll_mlcsr_sp.h"
#include "llama/ll_mlcsr_properties.h"

#define LL_ADVISOR_NODE 0
#define LL_ADVISOR_COMPLETE 1
#define LL_ADVISOR_SEQUENTIAL 2

template <class Graph, bool async=true, int flag=LL_ADVISOR_COMPLETE>
class ll_advisor {

private:
	Graph *graph;

	struct madvise_queue_item {
		node_t node;
		unsigned int epoch;
	};

	bool still_adding = true;
	unsigned int epoch = 0;
	deque<madvise_queue_item> madvise_queue;
	pthread_t madvise_thread;
	pthread_mutex_t madvise_lock;

	static void* madvise_thread_func(void* arg) {
		ll_advisor<Graph, async, flag> *advisor = (ll_advisor<Graph, async, flag> *)(arg);
		auto vtable = advisor->graph->out().vertex_table(0);
		auto etable = advisor->graph->out().edge_table(0);
		int skip_count = 0;
		int node_count = 0;
		int advise_count = 0;
		int second_skip_count = 0;
		while (advisor->still_adding) {
			node_t add;
			unsigned int epoch;
			pthread_mutex_lock(&advisor->madvise_lock);
			if (advisor->madvise_queue.empty()) {
				pthread_mutex_unlock(&advisor->madvise_lock);
				continue;
			}
			madvise_queue_item item = advisor->madvise_queue.front();
			add = item.node;
			epoch = item.epoch;
			advisor->madvise_queue.pop_front();
			pthread_mutex_unlock(&advisor->madvise_lock);
			node_count++;

			edge_t first = (*vtable)[add].adj_list_start;
			edge_t last = first + (*vtable)[add].level_length;

			if (flag == LL_ADVISOR_SEQUENTIAL) {
				/*if (advisor->epoch - epoch > 0) {skip_count++; continue;}
				node_t lag = 1024 * 1024 / sizeof(node_t);*/
				etable->advise(last + (3<<20)/sizeof(edge_t), last + (4<<20)/sizeof(edge_t));
				advise_count++;
				continue;
			}

			if (advisor->epoch - epoch > 4) {skip_count++; continue;}
			if (flag == LL_ADVISOR_COMPLETE) {
				//#pragma omp parallel num_threads(8)
				{
					//#pragma omp for
					for (edge_t edge_var = first; edge_var <  last; edge_var++) {
						node_t* next_ptr = etable->edge_ptr(add, LL_EDGE_INDEX(edge_var));
						if (next_ptr == NULL) continue;
						node_t next = *next_ptr;
						if (advisor->epoch - epoch > 5) {
							second_skip_count++;
							break;
						}
						if (!advisor->still_adding) continue;

						edge_t next_first = (*vtable)[next].adj_list_start;
						edge_t next_last = next_first + (*vtable)[next].level_length;
						if (next_last - next_first < 500) continue;
						etable->advise(next_first, next_last);
						advise_count ++;
					}
				}
			}
		}
		printf("Encountered %d nodes, advised %d nodes, skipped %d nodes, %d second time\n",
				node_count, advise_count, skip_count, second_skip_count);
		return NULL;
	}

public:
	ll_advisor(Graph *_graph) {
		printf("Starting advisor\n");
		graph = _graph;
		if (async) {
			pthread_mutex_init(&madvise_lock, NULL);
			pthread_create(&madvise_thread, NULL,
					&ll_advisor<Graph, async, flag>::madvise_thread_func, (void*)this);
		}
	}

	void advise(node_t node) {
		if (async) {
			pthread_mutex_lock(&madvise_lock);
			madvise_queue.push_back({node, ++epoch});
			pthread_mutex_unlock(&madvise_lock);
		} else {
			auto vtable = graph->out().vertex_table(0);
			auto etable = graph->out().edge_table(0);
			edge_t first = (*vtable)[node].adj_list_start;
			edge_t last = first + (*vtable)[node].level_length;
			if (last - first > 0) {
				etable->advise(first, last);
			}
			if (flag == LL_ADVISOR_COMPLETE) {
				ll_edge_iterator it;
				graph->out_iter_begin(it, node, 0);

				FOREACH_OUTEDGE_ITER(v_idx, *graph, it) {
					node_t next = it.last_node;
					edge_t next_first = (*vtable)[next].adj_list_start;
					edge_t next_last = next_first + (*vtable)[next].level_length;
					if (next_last - next_first > 0)
						etable->advise(next_first, next_last);
				}
			}
		}

	}

	void stop() {
		if (async) {
			still_adding = false;
			pthread_join(madvise_thread, NULL);
			pthread_mutex_destroy(&madvise_lock);
		}
	}
};

#endif
