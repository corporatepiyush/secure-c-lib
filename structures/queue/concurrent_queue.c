#include "concurrent_queue.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_atomic_queue_init(scl_allocator_t *alloc, scl_atomic_queue_t *queue, size_t element_size)
{
    if (!queue) return SCL_ERR_NULL_PTR;
    if (element_size == 0) return SCL_ERR_INVALID_ARG;
    scl_atomic_queue_node_t *dummy = scl_alloc(alloc, sizeof(scl_atomic_queue_node_t), alignof(max_align_t));
    if (!dummy) return SCL_ERR_OUT_OF_MEMORY;
    dummy->data = NULL;
    atomic_init(&dummy->next, (uintptr_t)NULL);
    atomic_init(&queue->head, (uintptr_t)dummy);
    atomic_init(&queue->tail, (uintptr_t)dummy);
    atomic_init(&queue->count, 0);
    queue->element_size = element_size;
    return SCL_OK;
}

void scl_atomic_queue_destroy(scl_allocator_t *alloc, scl_atomic_queue_t *queue)
{
    if (!queue) return;
    scl_atomic_queue_node_t *cur = (scl_atomic_queue_node_t *)atomic_load_explicit(&queue->head, memory_order_acquire);
    while (cur) {
        scl_atomic_queue_node_t *next = (scl_atomic_queue_node_t *)atomic_load_explicit(&cur->next, memory_order_relaxed);
        scl_free(alloc, cur->data);
        scl_free(alloc, cur);
        cur = next;
    }
    atomic_store_explicit(&queue->head, (uintptr_t)NULL, memory_order_relaxed);
    atomic_store_explicit(&queue->tail, (uintptr_t)NULL, memory_order_relaxed);
    atomic_store_explicit(&queue->count, 0, memory_order_relaxed);
}

scl_error_t scl_atomic_queue_enqueue(scl_allocator_t *alloc, scl_atomic_queue_t *queue, const void *element)
{
    if (!queue || !element) return SCL_ERR_NULL_PTR;
    scl_atomic_queue_node_t *node = scl_alloc(alloc, sizeof(scl_atomic_queue_node_t), alignof(max_align_t));
    if (!node) return SCL_ERR_OUT_OF_MEMORY;
    node->data = scl_alloc(alloc, queue->element_size, alignof(max_align_t));
    if (!node->data) {
        scl_free(alloc, node);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    memcpy(node->data, element, queue->element_size);
    atomic_init(&node->next, (uintptr_t)NULL);

    while (1) {
        scl_atomic_queue_node_t *tail = (scl_atomic_queue_node_t *)atomic_load_explicit(&queue->tail, memory_order_acquire);
        scl_atomic_queue_node_t *next = (scl_atomic_queue_node_t *)atomic_load_explicit(&tail->next, memory_order_acquire);
        if (tail != (scl_atomic_queue_node_t *)atomic_load_explicit(&queue->tail, memory_order_acquire))
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
    scl_atomic_queue_node_t *old_tail = (scl_atomic_queue_node_t *)atomic_load_explicit(&queue->tail, memory_order_relaxed);
    atomic_compare_exchange_weak_explicit(&queue->tail,
        (uintptr_t *)&old_tail, (uintptr_t)node,
        memory_order_release, memory_order_relaxed);
    atomic_fetch_add_explicit(&queue->count, 1, memory_order_relaxed);
    return SCL_OK;
}

scl_error_t scl_atomic_queue_dequeue(scl_allocator_t *alloc, scl_atomic_queue_t *queue, void *out)
{
    if (!queue || !out) return SCL_ERR_NULL_PTR;
    while (1) {
        scl_atomic_queue_node_t *head = (scl_atomic_queue_node_t *)atomic_load_explicit(&queue->head, memory_order_acquire);
        scl_atomic_queue_node_t *tail = (scl_atomic_queue_node_t *)atomic_load_explicit(&queue->tail, memory_order_acquire);
        scl_atomic_queue_node_t *next = (scl_atomic_queue_node_t *)atomic_load_explicit(&head->next, memory_order_acquire);

        if (head != (scl_atomic_queue_node_t *)atomic_load_explicit(&queue->head, memory_order_acquire))
            continue;

        if (head == tail) {
            if (next == NULL) return SCL_ERR_EMPTY;
            atomic_compare_exchange_weak_explicit(&queue->tail,
                (uintptr_t *)&tail, (uintptr_t)next,
                memory_order_release, memory_order_relaxed);
        } else {
            if (next == NULL) continue;
            memcpy(out, next->data, queue->element_size);
            uintptr_t old_head = (uintptr_t)head;
            if (atomic_compare_exchange_weak_explicit(&queue->head, &old_head, (uintptr_t)next,
                    memory_order_acquire, memory_order_relaxed)) {
                scl_free(alloc, head->data);
                scl_free(alloc, head);
                atomic_fetch_sub_explicit(&queue->count, 1, memory_order_relaxed);
                return SCL_OK;
            }
        }
    }
}

size_t scl_atomic_queue_count(const scl_atomic_queue_t *queue)
{
    return queue ? atomic_load_explicit(&queue->count, memory_order_relaxed) : 0;
}

bool scl_atomic_queue_empty(const scl_atomic_queue_t *queue)
{
    if (!queue) return true;
    scl_atomic_queue_node_t *head = (scl_atomic_queue_node_t *)atomic_load_explicit(&queue->head, memory_order_acquire);
    scl_atomic_queue_node_t *next = (scl_atomic_queue_node_t *)atomic_load_explicit(&head->next, memory_order_acquire);
    return next == NULL;
}
