#include "concurrent_slist.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_concurrent_slist_init(scl_concurrent_slist_t *list, size_t element_size)
{
    if (!list) return SCL_ERR_NULL_PTR;
    if (element_size == 0) return SCL_ERR_INVALID_ARG;
    atomic_init(&list->head, (uintptr_t)NULL);
    atomic_init(&list->count, 0);
    list->element_size = element_size;
    return SCL_OK;
}

void scl_concurrent_slist_destroy(scl_concurrent_slist_t *list)
{
    if (!list) return;
    scl_concurrent_slist_node_t *cur = (scl_concurrent_slist_node_t *)atomic_load_explicit(&list->head, memory_order_acquire);
    while (cur) {
        scl_concurrent_slist_node_t *next = cur->next;
        free(cur->data);
        free(cur);
        cur = next;
    }
    atomic_store_explicit(&list->head, (uintptr_t)NULL, memory_order_relaxed);
    atomic_store_explicit(&list->count, 0, memory_order_relaxed);
}

scl_error_t scl_concurrent_slist_push_front(scl_concurrent_slist_t *list, const void *element)
{
    if (!list || !element) return SCL_ERR_NULL_PTR;
    scl_concurrent_slist_node_t *node = malloc(sizeof(scl_concurrent_slist_node_t));
    if (!node) return SCL_ERR_OUT_OF_MEMORY;
    node->data = malloc(list->element_size);
    if (!node->data) {
        free(node);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    memcpy(node->data, element, list->element_size);

    scl_concurrent_slist_node_t *old_head = (scl_concurrent_slist_node_t *)atomic_load_explicit(&list->head, memory_order_relaxed);
    do {
        node->next = old_head;
    } while (!atomic_compare_exchange_weak_explicit(&list->head,
             (uintptr_t *)&old_head, (uintptr_t)node,
             memory_order_release, memory_order_relaxed));

    atomic_fetch_add_explicit(&list->count, 1, memory_order_relaxed);
    return SCL_OK;
}

scl_error_t scl_concurrent_slist_pop_front(scl_concurrent_slist_t *list, void *out)
{
    if (!list || !out) return SCL_ERR_NULL_PTR;
    scl_concurrent_slist_node_t *old_head = (scl_concurrent_slist_node_t *)atomic_load_explicit(&list->head, memory_order_relaxed);
    while (1) {
        if (!old_head) return SCL_ERR_EMPTY;
        if (atomic_compare_exchange_weak_explicit(&list->head,
                (uintptr_t *)&old_head, (uintptr_t)old_head->next,
                memory_order_acquire, memory_order_relaxed))
            break;
    }
    memcpy(out, old_head->data, list->element_size);
    free(old_head->data);
    free(old_head);
    atomic_fetch_sub_explicit(&list->count, 1, memory_order_relaxed);
    return SCL_OK;
}

size_t scl_concurrent_slist_count(const scl_concurrent_slist_t *list)
{
    return list ? atomic_load_explicit(&list->count, memory_order_relaxed) : 0;
}

bool scl_concurrent_slist_empty(const scl_concurrent_slist_t *list)
{
    return list ? atomic_load_explicit(&list->count, memory_order_relaxed) == 0 : true;
}
