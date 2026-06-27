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

/* Thread-safe slist data structure. Guarded by scl_spinlock_t. */

#include "scl_concurrent_slist.h"
#include "scl_string.h"

scl_error_t scl_cslist_init(scl_allocator_t *alloc, scl_concurrent_slist_t *list, size_t element_size)
{
    (void)alloc;
    if (scl_unlikely(!list)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(element_size == 0)) return SCL_ERR_INVALID_ARG;
    atomic_init(&list->head, scl_tagged_make(NULL, 0));
    atomic_init(&list->count, 0);
    list->element_size = element_size;
    return SCL_OK;
}

void scl_cslist_destroy(scl_allocator_t *alloc, scl_concurrent_slist_t *list)
{
    if (scl_unlikely(!list)) return;
    scl_tagged_ptr_t tp = atomic_load_explicit(&list->head, memory_order_acquire);
    scl_concurrent_slist_node_t *cur = (scl_concurrent_slist_node_t *)tp.ptr;
    while (scl_likely(cur)) {
        scl_concurrent_slist_node_t *next = cur->next;
        scl_free(alloc, cur->data);
        scl_free(alloc, cur);
        cur = next;
    }
    atomic_store_explicit(&list->head, scl_tagged_make(NULL, 0), memory_order_relaxed);
    atomic_store_explicit(&list->count, 0, memory_order_relaxed);
}

scl_error_t scl_cslist_push_front(scl_allocator_t *alloc, scl_concurrent_slist_t *list, const void  *SCL_RESTRICT element)
{
    if (scl_unlikely(!list || !element)) return SCL_ERR_NULL_PTR;
    scl_concurrent_slist_node_t *node = scl_alloc(alloc, sizeof(scl_concurrent_slist_node_t), alignof(max_align_t));
    if (scl_unlikely(!node)) return SCL_ERR_OUT_OF_MEMORY;
    node->data = scl_alloc(alloc, list->element_size, alignof(max_align_t));
    if (scl_unlikely(!node->data)) {
        scl_free(alloc, node);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    scl_memcpy(node->data, element, list->element_size);

    scl_tagged_ptr_t old = atomic_load_explicit(&list->head, memory_order_relaxed);
    do {
        node->next = old.ptr;
    } while (!atomic_compare_exchange_weak_explicit(&list->head, &old,
             scl_tagged_make(node, old.tag + 1),
             memory_order_release, memory_order_relaxed));

    atomic_fetch_add_explicit(&list->count, 1, memory_order_relaxed);
    return SCL_OK;
}

scl_error_t scl_cslist_pop_front(scl_allocator_t *alloc, scl_concurrent_slist_t *list, void  *SCL_RESTRICT out)
{
    if (scl_unlikely(!list || !out)) return SCL_ERR_NULL_PTR;
    scl_tagged_ptr_t old = atomic_load_explicit(&list->head, memory_order_relaxed);
    while (1) {
        if (scl_unlikely(!old.ptr)) return SCL_ERR_EMPTY;
        scl_concurrent_slist_node_t *next = ((scl_concurrent_slist_node_t *)old.ptr)->next;
        if (atomic_compare_exchange_weak_explicit(&list->head, &old,
                scl_tagged_make(next, old.tag + 1),
                memory_order_acquire, memory_order_relaxed))
            break;
    }
    scl_memcpy(out, ((scl_concurrent_slist_node_t *)old.ptr)->data, list->element_size);
    scl_free(alloc, ((scl_concurrent_slist_node_t *)old.ptr)->data);
    scl_free(alloc, (scl_concurrent_slist_node_t *)old.ptr);
    atomic_fetch_sub_explicit(&list->count, 1, memory_order_relaxed);
    return SCL_OK;
}

size_t scl_cslist_count(const scl_concurrent_slist_t *list)
{
    return list ? atomic_load_explicit(&list->count, memory_order_relaxed) : 0;
}

bool scl_cslist_empty(const scl_concurrent_slist_t *list)
{
    scl_tagged_ptr_t tp;
    if (scl_unlikely(!list)) return true;
    tp = atomic_load_explicit(&list->head, memory_order_relaxed);
    return tp.ptr == NULL;
}
