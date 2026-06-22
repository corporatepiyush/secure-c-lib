#include "concurrent_queue.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_concurrent_queue_init(scl_concurrent_queue_t *queue, size_t element_size)
{
    if (!queue) return SCL_ERR_NULL_PTR;
    if (element_size == 0) return SCL_ERR_INVALID_ARG;
    scl_concurrent_queue_node_t *dummy = malloc(sizeof(scl_concurrent_queue_node_t));
    if (!dummy) return SCL_ERR_OUT_OF_MEMORY;
    dummy->data = NULL;
    atomic_init(&dummy->next, (uintptr_t)NULL);
    atomic_init(&queue->head, (uintptr_t)dummy);
    atomic_init(&queue->tail, (uintptr_t)dummy);
    atomic_init(&queue->count, 0);
    queue->element_size = element_size;
    return SCL_OK;
}

void scl_concurrent_queue_destroy(scl_concurrent_queue_t *queue)
{
    if (!queue) return;
    scl_concurrent_queue_node_t *cur = (scl_concurrent_queue_node_t *)atomic_load_explicit(&queue->head, memory_order_acquire);
    while (cur) {
        scl_concurrent_queue_node_t *next = (scl_concurrent_queue_node_t *)atomic_load_explicit(&cur->next, memory_order_relaxed);
        free(cur->data);
        free(cur);
        cur = next;
    }
    atomic_store_explicit(&queue->head, (uintptr_t)NULL, memory_order_relaxed);
    atomic_store_explicit(&queue->tail, (uintptr_t)NULL, memory_order_relaxed);
    atomic_store_explicit(&queue->count, 0, memory_order_relaxed);
}

scl_error_t scl_concurrent_queue_enqueue(scl_concurrent_queue_t *queue, const void *element)
{
    if (!queue || !element) return SCL_ERR_NULL_PTR;
    scl_concurrent_queue_node_t *node = malloc(sizeof(scl_concurrent_queue_node_t));
    if (!node) return SCL_ERR_OUT_OF_MEMORY;
    node->data = malloc(queue->element_size);
    if (!node->data) {
        free(node);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    memcpy(node->data, element, queue->element_size);
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

scl_error_t scl_concurrent_queue_dequeue(scl_concurrent_queue_t *queue, void *out)
{
    if (!queue || !out) return SCL_ERR_NULL_PTR;
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
            memcpy(out, next->data, queue->element_size);
            uintptr_t old_head = (uintptr_t)head;
            if (atomic_compare_exchange_weak_explicit(&queue->head, &old_head, (uintptr_t)next,
                    memory_order_acquire, memory_order_relaxed)) {
                free(head->data);
                free(head);
                atomic_fetch_sub_explicit(&queue->count, 1, memory_order_relaxed);
                return SCL_OK;
            }
        }
    }
}

size_t scl_concurrent_queue_count(const scl_concurrent_queue_t *queue)
{
    return queue ? atomic_load_explicit(&queue->count, memory_order_relaxed) : 0;
}

bool scl_concurrent_queue_empty(const scl_concurrent_queue_t *queue)
{
    if (!queue) return true;
    scl_concurrent_queue_node_t *head = (scl_concurrent_queue_node_t *)atomic_load_explicit(&queue->head, memory_order_acquire);
    scl_concurrent_queue_node_t *next = (scl_concurrent_queue_node_t *)atomic_load_explicit(&head->next, memory_order_acquire);
    return next == NULL;
}
