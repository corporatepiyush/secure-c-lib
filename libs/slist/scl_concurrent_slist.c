#include "scl_concurrent_slist.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_cslist_init(scl_allocator_t *alloc, scl_concurrent_slist_t *list, size_t element_size)
{
    (void)alloc;
    if (!list) return SCL_ERR_NULL_PTR;
    if (element_size == 0) return SCL_ERR_INVALID_ARG;
    atomic_init(&list->head, (uintptr_t)NULL);
    atomic_init(&list->count, 0);
    list->element_size = element_size;
    return SCL_OK;
}

void scl_cslist_destroy(scl_allocator_t *alloc, scl_concurrent_slist_t *list)
{
    if (!list) return;
    scl_concurrent_slist_node_t *cur = (scl_concurrent_slist_node_t *)atomic_load_explicit(&list->head, memory_order_acquire);
    while (cur) {
        scl_concurrent_slist_node_t *next = cur->next;
        scl_free(alloc, cur->data);
        scl_free(alloc, cur);
        cur = next;
    }
    atomic_store_explicit(&list->head, (uintptr_t)NULL, memory_order_relaxed);
    atomic_store_explicit(&list->count, 0, memory_order_relaxed);
}

scl_error_t scl_cslist_push_front(scl_allocator_t *alloc, scl_concurrent_slist_t *list, const void *element)
{
    if (!list || !element) return SCL_ERR_NULL_PTR;
    scl_concurrent_slist_node_t *node = scl_alloc(alloc, sizeof(scl_concurrent_slist_node_t), alignof(max_align_t));
    if (!node) return SCL_ERR_OUT_OF_MEMORY;
    node->data = scl_alloc(alloc, list->element_size, alignof(max_align_t));
    if (!node->data) {
        scl_free(alloc, node);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    scl_memcpy(node->data, element, list->element_size);

    scl_concurrent_slist_node_t *old_head = (scl_concurrent_slist_node_t *)atomic_load_explicit(&list->head, memory_order_relaxed);
    do {
        node->next = old_head;
    } while (!atomic_compare_exchange_weak_explicit(&list->head,
             (uintptr_t *)&old_head, (uintptr_t)node,
             memory_order_release, memory_order_relaxed));

    atomic_fetch_add_explicit(&list->count, 1, memory_order_relaxed);
    return SCL_OK;
}

scl_error_t scl_cslist_pop_front(scl_allocator_t *alloc, scl_concurrent_slist_t *list, void *out)
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
    scl_memcpy(out, old_head->data, list->element_size);
    scl_free(alloc, old_head->data);
    scl_free(alloc, old_head);
    atomic_fetch_sub_explicit(&list->count, 1, memory_order_relaxed);
    return SCL_OK;
}

size_t scl_cslist_count(const scl_concurrent_slist_t *list)
{
    return list ? atomic_load_explicit(&list->count, memory_order_relaxed) : 0;
}

bool scl_cslist_empty(const scl_concurrent_slist_t *list)
{
    return list ? atomic_load_explicit(&list->count, memory_order_relaxed) == 0 : true;
}
