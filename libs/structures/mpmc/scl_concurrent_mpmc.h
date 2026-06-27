/*
 * Copyright 2026 Piyush Katariya
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Vyukov bounded MPMC queue. Lock-free, power-of-2 sized. Cache-line padded enqueue/dequeue. O(1) bounded capacity. */

#ifndef SCL_CONCURRENT_MPMC_H
#define SCL_CONCURRENT_MPMC_H

/*
 * scl_mpmc_queue — lock-free bounded multi-producer/multi-consumer FIFO
 * (Vyukov algorithm) over a power-of-two ring.
 *
 * Each cell carries a sequence number, so progress uses only single-word CAS
 * and the design is immune to ABA without tagging. enqueue/dequeue are
 * wait-free per attempt and lock-free overall; "full"/"empty" are reported
 * rather than blocked on. Cells hold a uintptr_t (a pointer or an fd).
 */

#include "scl_common.h"
#include "scl_atomic.h"

typedef struct {
    _Alignas(SCL_CACHE_LINE_SIZE) scl_atomic_size_t seq;
    uintptr_t value;
} scl_mpmc_cell_t;

typedef struct {
    scl_mpmc_cell_t *buffer;
    size_t           mask;
    scl_allocator_t *alloc;
    _Alignas(SCL_CACHE_LINE_SIZE) scl_atomic_size_t enqueue_pos;
    _Alignas(SCL_CACHE_LINE_SIZE) scl_atomic_size_t dequeue_pos;
} scl_mpmc_queue_t;

static inline scl_error_t scl_mpmc_init(scl_allocator_t *alloc,
                                        scl_mpmc_queue_t *q, size_t capacity) {
    if (scl_unlikely(!q || !alloc)) return SCL_ERR_NULL_PTR;
    if (capacity < 2) capacity = 2;
    capacity = scl_bit_ceil_sz(capacity);     /* round up to power of two */
    q->buffer = (scl_mpmc_cell_t *)scl_calloc(alloc, capacity,
                    sizeof(scl_mpmc_cell_t), _Alignof(scl_mpmc_cell_t));
    if (!q->buffer) return SCL_ERR_OUT_OF_MEMORY;
    q->mask  = capacity - 1;
    q->alloc = alloc;
    for (size_t i = 0; i < capacity; i++)
        atomic_init(&q->buffer[i].seq, i);
    atomic_init(&q->enqueue_pos, 0);
    atomic_init(&q->dequeue_pos, 0);
    return SCL_OK;
}

static inline void scl_mpmc_destroy(scl_mpmc_queue_t *q) {
    if (!q || !q->buffer) return;
    scl_free(q->alloc, q->buffer);
    q->buffer = NULL;
}

/* Returns true on success, false if the queue is full. */
static inline bool scl_mpmc_enqueue(scl_mpmc_queue_t *q, uintptr_t value) {
    scl_mpmc_cell_t *cell;
    size_t pos = atomic_load_explicit(&q->enqueue_pos, memory_order_relaxed);
    for (;;) {
        cell = &q->buffer[pos & q->mask];
        size_t seq = atomic_load_explicit(&cell->seq, memory_order_acquire);
        intptr_t dif = (intptr_t)seq - (intptr_t)pos;
        if (dif == 0) {
            if (atomic_compare_exchange_weak_explicit(&q->enqueue_pos, &pos,
                    pos + 1, memory_order_relaxed, memory_order_relaxed))
                break;
        } else if (dif < 0) {
            return false;                     /* full */
        } else {
            pos = atomic_load_explicit(&q->enqueue_pos, memory_order_relaxed);
        }
    }
    cell->value = value;
    atomic_store_explicit(&cell->seq, pos + 1, memory_order_release);
    return true;
}

/* Returns true and writes *out on success, false if the queue is empty. */
static inline bool scl_mpmc_dequeue(scl_mpmc_queue_t *q, uintptr_t *out) {
    scl_mpmc_cell_t *cell;
    size_t pos = atomic_load_explicit(&q->dequeue_pos, memory_order_relaxed);
    for (;;) {
        cell = &q->buffer[pos & q->mask];
        size_t seq = atomic_load_explicit(&cell->seq, memory_order_acquire);
        intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
        if (dif == 0) {
            if (atomic_compare_exchange_weak_explicit(&q->dequeue_pos, &pos,
                    pos + 1, memory_order_relaxed, memory_order_relaxed))
                break;
        } else if (dif < 0) {
            return false;                     /* empty */
        } else {
            pos = atomic_load_explicit(&q->dequeue_pos, memory_order_relaxed);
        }
    }
    *out = cell->value;
    atomic_store_explicit(&cell->seq, pos + q->mask + 1, memory_order_release);
    return true;
}

#endif /* SCL_CONCURRENT_MPMC_H */
