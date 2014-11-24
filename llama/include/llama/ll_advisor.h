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

			if (advisor->epoch - epoch > PARAM_EPOCH_THRESHOLD) continue;

			auto vtable = advisor->graph->out().vertex_table(0);
			auto etable = advisor->graph->out().edge_table(0);
			edge_t first = (*vtable)[add].adj_list_start;
			edge_t last = first + (*vtable)[add].level_length;

			if (first < last) {
				etable->advise(first, last);
			}

			if (flag == LL_ADVISOR_COMPLETE) {
				ll_edge_iterator it;
				advisor->graph->out_iter_begin(it, add, 0);

				FOREACH_OUTEDGE_ITER(v_idx, *advisor->graph, it) {
					if (!advisor->still_adding) break;
					node_t next = it.last_node;
					if (advisor->epoch - epoch > PARAM_EPOCH_THRESHOLD) continue;

					unsigned degree = (*vtable)[next].level_length;
					if (degree < 200000) continue;
					edge_t next_first = (*vtable)[next].adj_list_start;
					edge_t next_last = next_first + degree;
					if (next_first < next_last) {
						etable->advise(next_first, next_last);
					}
				}
			}
		}
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
			madvise_queue.push_back({node, epoch});
			epoch++;
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
