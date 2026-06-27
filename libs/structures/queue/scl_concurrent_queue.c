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

/* Thread-safe queue data structure. Guarded by scl_spinlock_t. */

#include "scl_concurrent_queue.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_cqueue_init(scl_allocator_t *alloc, scl_concurrent_queue_t *queue, size_t element_size)
{
    if (scl_unlikely(!queue)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(element_size == 0)) return SCL_ERR_INVALID_ARG;
    scl_concurrent_queue_node_t *dummy = scl_alloc(alloc, sizeof(scl_concurrent_queue_node_t), alignof(max_align_t));
    if (scl_unlikely(!dummy)) return SCL_ERR_OUT_OF_MEMORY;
    dummy->data = NULL;
    atomic_init(&dummy->next, (uintptr_t)NULL);
    atomic_init(&queue->head, (uintptr_t)dummy);
    atomic_init(&queue->tail, (uintptr_t)dummy);
    atomic_init(&queue->count, 0);
    queue->element_size = element_size;
    return SCL_OK;
}

void scl_cqueue_destroy(scl_allocator_t *alloc, scl_concurrent_queue_t *queue)
{
    if (scl_unlikely(!queue)) return;
    size_t esz = queue->element_size;
    scl_concurrent_queue_node_t *cur = (scl_concurrent_queue_node_t *)atomic_load_explicit(&queue->head, memory_order_acquire);
    while (scl_likely(cur)) {
        scl_concurrent_queue_node_t *next = (scl_concurrent_queue_node_t *)atomic_load_explicit(&cur->next, memory_order_relaxed);
        if (cur->data) scl_secure_zero(cur->data, esz);
        scl_free(alloc, cur->data);
        scl_free(alloc, cur);
        cur = next;
    }
    atomic_store_explicit(&queue->head, (uintptr_t)NULL, memory_order_relaxed);
    atomic_store_explicit(&queue->tail, (uintptr_t)NULL, memory_order_relaxed);
    atomic_store_explicit(&queue->count, 0, memory_order_relaxed);
}

scl_error_t scl_cqueue_enqueue(scl_allocator_t *alloc, scl_concurrent_queue_t *queue, const void  *SCL_RESTRICT element)
{
    if (scl_unlikely(!queue || !element)) return SCL_ERR_NULL_PTR;
    scl_concurrent_queue_node_t *node = scl_alloc(alloc, sizeof(scl_concurrent_queue_node_t), alignof(max_align_t));
    if (scl_unlikely(!node)) return SCL_ERR_OUT_OF_MEMORY;
    node->data = scl_alloc(alloc, queue->element_size, alignof(max_align_t));
    if (scl_unlikely(!node->data)) {
        scl_free(alloc, node);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    scl_memcpy(node->data, element, queue->element_size);
    atomic_init(&node->next, (uintptr_t)NULL);

    while (1) {
        scl_concurrent_queue_node_t *tail = (scl_concurrent_queue_node_t *)atomic_load_explicit(&queue->tail, memory_order_acquire);
        scl_concurrent_queue_node_t *next = (scl_concurrent_queue_node_t *)atomic_load_explicit(&tail->next, memory_order_acquire);
        if (tail != (scl_concurrent_queue_node_t *)atomic_load_explicit(&queue->tail, memory_order_acquire))
            continue;
        if (next == NULL) {
            uintptr_t expected = (uintptr_t)NULL;
            if (atomic_compare_exchange_weak_explicit(&tail->next, &expected, (uintptr_t)node,
                    memory_order_release, memory_order_relaxed))
                break;
        } else {
            atomic_compare_exchange_weak_explicit(&queue->tail,
                (uintptr_t *)&tail, (uintptr_t)next,
                memory_order_release, memory_order_relaxed);
        }
    }
    scl_concurrent_queue_node_t *old_tail = (scl_concurrent_queue_node_t *)atomic_load_explicit(&queue->tail, memory_order_relaxed);
    atomic_compare_exchange_weak_explicit(&queue->tail,
        (uintptr_t *)&old_tail, (uintptr_t)node,
        memory_order_release, memory_order_relaxed);
    atomic_fetch_add_explicit(&queue->count, 1, memory_order_relaxed);
    return SCL_OK;
}

scl_error_t scl_cqueue_dequeue(scl_allocator_t *alloc, scl_concurrent_queue_t *queue, void  *SCL_RESTRICT out)
{
    if (scl_unlikely(!queue || !out)) return SCL_ERR_NULL_PTR;
    while (1) {
        scl_concurrent_queue_node_t *head = (scl_concurrent_queue_node_t *)atomic_load_explicit(&queue->head, memory_order_acquire);
        scl_concurrent_queue_node_t *tail = (scl_concurrent_queue_node_t *)atomic_load_explicit(&queue->tail, memory_order_acquire);
        scl_concurrent_queue_node_t *next = (scl_concurrent_queue_node_t *)atomic_load_explicit(&head->next, memory_order_acquire);

        if (head != (scl_concurrent_queue_node_t *)atomic_load_explicit(&queue->head, memory_order_acquire))
            continue;

        if (head == tail) {
            if (next == NULL) return SCL_ERR_EMPTY;
            atomic_compare_exchange_weak_explicit(&queue->tail,
                (uintptr_t *)&tail, (uintptr_t)next,
                memory_order_release, memory_order_relaxed);
        } else {
            if (next == NULL) continue;
            scl_memcpy(out, next->data, queue->element_size);
            uintptr_t old_head = (uintptr_t)head;
            if (atomic_compare_exchange_weak_explicit(&queue->head, &old_head, (uintptr_t)next,
                    memory_order_acq_rel, memory_order_relaxed)) {
                scl_free(alloc, head->data);
                scl_free(alloc, head);
                atomic_fetch_sub_explicit(&queue->count, 1, memory_order_relaxed);
                return SCL_OK;
            }
        }
    }
}

size_t scl_cqueue_count(const scl_concurrent_queue_t *queue)
{
    return queue ? atomic_load_explicit(&queue->count, memory_order_relaxed) : 0;
}

bool scl_cqueue_empty(const scl_concurrent_queue_t *queue)
{
    if (scl_unlikely(!queue)) return true;
    scl_concurrent_queue_node_t *head = (scl_concurrent_queue_node_t *)atomic_load_explicit(&queue->head, memory_order_acquire);
    scl_concurrent_queue_node_t *next = (scl_concurrent_queue_node_t *)atomic_load_explicit(&head->next, memory_order_acquire);
    return next == NULL;
}
