/*
 * ll_mlcsr_iterator.h
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


#ifndef LL_MLCSR_ITERATOR_H_
#define LL_MLCSR_ITERATOR_H_

#include "llama/ll_common.h"
#include <deque>
#include <pthread.h>

//==========================================================================//
// Support: ll_edge_iterator                                                //
//==========================================================================//

#define LL_I_OWNER_NONE			-1
#define LL_I_OWNER_RO_CSR		0
#define LL_I_OWNER_WRITABLE		1

using std::deque;
struct ll_mlcsr_core__begin_t;

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

        ~syncqueue() {
            // TODO(fding): This code is completely bogus.
            // It actually doesn't free up all the memory. Fix! (we'd need to walk the linked list)
            //
            // TODO(fding): It would be helpful to add logging information here,
            // to see how fast dequeuing occurs relative to enqueuing.
            /* node* curr_node = head;
            int count = 0;
            unsigned int min_epoch = UINT_MAX;
                unsigned int max_epoch = 0;
            while (curr_node != NULL) {
                count++;
                min_epoch = min(min_epoch, curr_node->epoch);
                max_epoch = max(max_epoch, curr_node->epoch);
                curr_node = curr_node->next;
            }

            printf("count: %d\n", count);
            printf("epoch difference: %u\n", max_epoch - min_epoch);
            if (tmp_head_section) {
                free(tmp_head_section);
            }*/
        }

        // TODO(fding): this function keeps checking head,
        // which is always changed by dequeue.
        // I don't think this is good for the cache,
        // as dequeue and enqueue probably runs on different cores.
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

struct madvise_queue_item {
	node_t node;
	unsigned int epoch;
};

struct ll_edge_iterator {

	edge_t edge;

	int owner;
	node_t node;

	size_t left;
	const void* ptr;

	node_t last_node;

#ifdef LL_DELETIONS
	size_t max_level;
#endif
};

//==========================================================================//
// Additional APIs                                                          //
//==========================================================================//

#define LL_ITER_OUT_NEXT_NODE(graph, iter, last_edge)		((iter).last_node)
#define LL_ITER_IN_NEXT_EDGE(graph, iter, last_edge)		((iter).last_node)

#define FOREACH_ITER(edge_var, graph, iter_var) \
	for (edge_t edge_var = (graph).iter_next(iter_var); \
			edge_var != LL_NIL_EDGE; \
			edge_var = (graph).iter_next(iter_var))

#define FOREACH_ITER_WITHIN_LEVEL(edge_var, graph, iter_var) \
	for (edge_t edge_var = (graph).iter_next_within_level(iter_var); \
			edge_var != LL_NIL_EDGE; \
			edge_var = (graph).iter_next_within_level(iter_var))

#define FOREACH_OUTEDGE_ITER(edge_var, graph, iter_var) \
	for (edge_t edge_var = (graph).out_iter_next(iter_var); \
			edge_var != LL_NIL_EDGE; \
			edge_var = (graph).out_iter_next(iter_var))

#define FOREACH_OUTEDGE_ITER_WITHIN_LEVEL(edge_var, graph, iter_var) \
	for (edge_t edge_var = (graph).out_iter_next_within_level(iter_var); \
			edge_var != LL_NIL_EDGE; \
			edge_var = (graph).out_iter_next_within_level(iter_var))

#define FOREACH_INEDGE_ITER(edge_var, graph, iter_var) \
	for (edge_t edge_var = (graph).in_iter_next(iter_var); \
			edge_var != LL_NIL_EDGE; \
			edge_var = (graph).in_iter_next(iter_var))

#define FOREACH_INNODE_ITER(node_var, graph, iter_var) \
	for (node_t node_var = (graph).inm_iter_next(iter_var); \
			node_var != LL_NIL_NODE; \
			node_var = (graph).inm_iter_next(iter_var))


//
// Node iterators - convenient, but with a minor runtime overhead
//

#define ll_foreach_node(node_var, graph) \
	for (node_t node_var = 0; node_var < (graph).max_nodes(); node_var++)

#define ll_foreach_node_omp(node_var, graph) \
	_Pragma("omp parallel for") \
	for (node_t node_var = 0; node_var < (graph).max_nodes(); node_var++)

#define ll_foreach_node_within_level(node_var, mlcsr, level) \
	ll_tmp_with_begin() \
	ll_tmp_with(auto ll_tmp_var(v) = (mlcsr).vertex_table(level)) \
	ll_tmp_with(ll_vertex_iterator ll_tmp_var(i)) \
	ll_tmp_with(node_t node_var = (ll_tmp_var(v)) \
			->modified_node_iter_begin_next(ll_tmp_var(i))) \
	for (ll_tmp_with_end(); \
			node_var != LL_NIL_EDGE; \
			node_var = (ll_tmp_var(v))->modified_node_iter_next(ll_tmp_var(i)))

#define ll_foreach_node_within_level_omp(node_var, mlcsr, level, block) \
	ll_tmp_with_begin() \
	ll_tmp_with(auto ll_tmp_var(v) = (mlcsr).vertex_table(level)) \
	for ( ; ll_tmp_var(b); ll_tmp_with_end()) \
	_Pragma("omp parallel for schedule(dynamic,1)") \
	for (node_t ll_tmp_var(s) = 0; \
			ll_tmp_var(s) < (node_t) (ll_tmp_var(v))->size(); \
			ll_tmp_var(s) += block) \
	ll_with(ll_vertex_iterator ll_tmp_var(i)) \
	for (node_t node_var \
			= (ll_tmp_var(v))->modified_node_iter_begin_next \
				(ll_tmp_var(i), ll_tmp_var(s), ll_tmp_var(s) + block); \
			node_var != LL_NIL_EDGE; \
			node_var = (ll_tmp_var(v))->modified_node_iter_next(ll_tmp_var(i)))


//
// Edge iterators - convenient, but with a minor runtime overhead
//

#define ll_foreach_edge(edge_var, node_var, mlcsr, source_node) \
	ll_tmp_with_begin() \
	ll_tmp_with(ll_edge_iterator ll_tmp_var(i)) \
	ll_tmp_with(edge_t edge_var = (mlcsr).iter_begin_next(ll_tmp_var(i), source_node)) \
	ll_tmp_with(node_t node_var = (ll_tmp_var(i)).last_node) \
	for (ll_tmp_with_end(); \
			edge_var != LL_NIL_EDGE; \
			edge_var = (mlcsr).iter_next(ll_tmp_var(i)), \
			node_var = (ll_tmp_var(i)).last_node)

#define ll_foreach_edge_within_level(edge_var, node_var, mlcsr, source_node, \
		level, ...) \
	ll_tmp_with_begin() \
	ll_tmp_with(ll_edge_iterator ll_tmp_var(i)) \
	ll_tmp_with((mlcsr).iter_begin_within_level(ll_tmp_var(i), source_node, \
				level, ## __VA_ARGS__)) \
	ll_tmp_with(edge_t edge_var = (mlcsr).iter_next_within_level(ll_tmp_var(i))) \
	ll_tmp_with(node_t node_var = (ll_tmp_var(i)).last_node) \
	for (ll_tmp_with_end(); \
			edge_var != LL_NIL_EDGE; \
			edge_var = (mlcsr).iter_next_within_level(ll_tmp_var(i)), \
			node_var = (ll_tmp_var(i)).last_node)

#define ll_foreach_out_ext(edge_var, node_var, graph, source_node) \
	ll_tmp_with_begin() \
	ll_tmp_with(ll_edge_iterator ll_tmp_var(i)) \
	ll_tmp_with(edge_t edge_var = (graph).out_iter_begin_next \
			(ll_tmp_var(i), source_node)) \
	ll_tmp_with(node_t node_var = (ll_tmp_var(i)).last_node) \
	for (ll_tmp_with_end(); \
			edge_var != LL_NIL_EDGE; \
			edge_var = (graph).out_iter_next(ll_tmp_var(i)), \
			node_var = (ll_tmp_var(i)).last_node)

#define ll_foreach_out(node_var, graph, source_node) \
	ll_tmp_with_begin() \
	ll_tmp_with(ll_edge_iterator ll_tmp_var(i)) \
	ll_tmp_with(edge_t ll_tmp_var(e) = (graph).out_iter_begin_next \
			(ll_tmp_var(i), source_node)) \
	ll_tmp_with(node_t node_var = (ll_tmp_var(i)).last_node) \
	for (ll_tmp_with_end(); \
			ll_tmp_var(e) != LL_NIL_EDGE; \
			ll_tmp_var(e) = (graph).out_iter_next(ll_tmp_var(i)), \
			node_var = (ll_tmp_var(i)).last_node)

#define ll_foreach_in_ext(edge_var, node_var, graph, source_node) \
	ll_tmp_with_begin() \
	ll_tmp_with(ll_edge_iterator ll_tmp_var(i)) \
	ll_tmp_with((graph).in_iter_begin(ll_tmp_var(i), source_node)) \
	ll_tmp_with(edge_t edge_var = (graph).in_iter_next(ll_tmp_var(i))) \
	ll_tmp_with(node_t node_var = (ll_tmp_var(i)).last_node) \
	for (ll_tmp_with_end(); \
			edge_var != LL_NIL_EDGE; \
			edge_var = (graph).in_iter_next(ll_tmp_var(i)), \
			node_var = (ll_tmp_var(i)).last_node)

#define ll_foreach_in(node_var, graph, source_node) \
	ll_tmp_with_begin() \
	ll_tmp_with(ll_edge_iterator ll_tmp_var(i)) \
	ll_tmp_with((graph).inm_iter_begin(ll_tmp_var(i), source_node)) \
	ll_tmp_with(node_t node_var = (graph).inm_iter_next(ll_tmp_var(i))) \
	for (ll_tmp_with_end(); \
			node_var != LL_NIL_NODE; \
			node_var = (graph).inm_iter_next(ll_tmp_var(i)))



//==========================================================================//
// Support: ll_common_neighbor_iter                                         //
//==========================================================================//

template <class EXP_GRAPH> class ll_common_neighbor_iter
{
	EXP_GRAPH& G;
	node_t src;
	node_t dst;

	ll_edge_iterator src_iter;
	ll_edge_iterator dst_iter;

	bool finished;

public:
	// graph, source, destination
	ll_common_neighbor_iter(EXP_GRAPH& _G, node_t s, node_t d)
		: G(_G), src(s), dst(d) {
		G.out_iter_begin(src_iter, src);
		G.out_iter_begin(dst_iter, dst);
		finished = false;
	}

	node_t get_next() {
		if (!G.out_iter_has_next(src_iter) || !G.out_iter_has_next(dst_iter))
			return LL_NIL_NODE;
		if (finished) return LL_NIL_NODE;
		do {
			edge_t e = G.out_iter_next(src_iter);
			if (e == LL_NIL_EDGE) {
				finished = true;
				break;
			}
			node_t t = LL_ITER_OUT_NEXT_NODE(G, src_iter, e);
			if (check_common(t)) return t;
		} while (!finished);
		return LL_NIL_NODE;
	}

private:
	bool check_common(node_t t) {
		// TODO The dst_iter restart belongs here if the edges are not sorted,
		// but if they are, it should probably go after the while loop. Please
		// check.
		//G.out_iter_end(dst_iter);
		//G.out_iter_begin(dst_iter, dst);
		while (true) {
			edge_t e = G.out_iter_next(dst_iter);
			if (e == LL_NIL_EDGE) break;
			node_t r = LL_ITER_OUT_NEXT_NODE(G, dst_iter, e);
			if (r == t) return true;
		}
		G.out_iter_end(dst_iter);
		G.out_iter_begin(dst_iter, dst);
		return false;
	}
};

template <class EXP_GRAPH> class ll_common_neighbor_iter_cxx
{
	EXP_GRAPH& G;
	node_t src;
	node_t dst;

	typename EXP_GRAPH::iterator src_iter;
	typename EXP_GRAPH::iterator dst_iter;
	typename EXP_GRAPH::iterator end;

	bool finished;

public:
	// graph, source, destination
	ll_common_neighbor_iter_cxx(EXP_GRAPH& _G, node_t s, node_t d)
		: G(_G), src(s), dst(d) {
		reset();
	}

	void reset() {
		src_iter = G.out_begin(src);
		dst_iter = G.out_begin(dst);
		end = G.end();
		finished = false;
	}

	node_t get_next() {
		if (src_iter == end || dst_iter == end || finished) return LL_NIL_NODE;
		do {
			if (src_iter == end) { finished = true; break; }
			node_t t = *src_iter;
			++src_iter;
			if (check_common(t)) return t;
		} while (!finished);
		return LL_NIL_NODE;
	}

private:
	bool check_common(node_t t) {
		//dst_iter = G.begin(dst);
		G.out_begin(dst_iter, dst);
		while (true) {
			if (dst_iter == end) break;
			node_t r = *dst_iter;
			++dst_iter;
			if (r == t) return true;
		}
		return false;
	}
};

#endif
