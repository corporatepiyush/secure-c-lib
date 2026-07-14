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

/* Thread-safe dlist data structure. Guarded by scl_spinlock_t. */

#include "scl_concurrent_dlist.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static scl_error_t create_node(scl_allocator_t *alloc,
                               scl_concurrent_dlist_node_t **out,
                               const void *data, size_t element_size) {
  scl_concurrent_dlist_node_t *node =
      scl_alloc(alloc, sizeof(scl_concurrent_dlist_node_t) + element_size,
                alignof(max_align_t));
  if (scl_unlikely(!node))
    return SCL_ERR_OUT_OF_MEMORY;
  scl_memcpy(node->data, data, element_size);
  node->prev = NULL;
  node->next = NULL;
  *out = node;
  return SCL_OK;
}

scl_error_t scl_cdlist_init(scl_allocator_t *alloc,
                            scl_concurrent_dlist_t *list, size_t element_size) {
  (void)alloc;
  if (scl_unlikely(!list))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(element_size == 0))
    return SCL_ERR_INVALID_ARG;
  list->head = NULL;
  list->tail = NULL;
  list->element_size = element_size;
  atomic_init(&list->count, 0);
  scl_spinlock_init(&list->lock);
  return SCL_OK;
}

void scl_cdlist_destroy(scl_allocator_t *alloc, scl_concurrent_dlist_t *list) {
  if (scl_unlikely(!list))
    return;
  scl_spinlock_lock(&list->lock);
  scl_concurrent_dlist_node_t *cur = list->head;
  while (scl_likely(cur)) {
    scl_concurrent_dlist_node_t *next = cur->next;
    scl_free(alloc, cur);
    cur = next;
  }
  list->head = NULL;
  list->tail = NULL;
  atomic_store_explicit(&list->count, 0, memory_order_relaxed);
  scl_spinlock_unlock(&list->lock);
}

scl_error_t scl_cdlist_push_front(scl_allocator_t *alloc,
                                  scl_concurrent_dlist_t *list,
                                  const void *SCL_RESTRICT element) {
  if (scl_unlikely(!list || !element))
    return SCL_ERR_NULL_PTR;
  scl_concurrent_dlist_node_t *node;
  scl_error_t err = create_node(alloc, &node, element, list->element_size);
  if (err != SCL_OK)
    return err;

  scl_spinlock_lock(&list->lock);
  node->next = list->head;
  if (list->head)
    list->head->prev = node;
  else
    list->tail = node;
  list->head = node;
  atomic_fetch_add_explicit(&list->count, 1, memory_order_relaxed);
  scl_spinlock_unlock(&list->lock);
  return SCL_OK;
}

scl_error_t scl_cdlist_push_back(scl_allocator_t *alloc,
                                 scl_concurrent_dlist_t *list,
                                 const void *SCL_RESTRICT element) {
  if (scl_unlikely(!list || !element))
    return SCL_ERR_NULL_PTR;
  scl_concurrent_dlist_node_t *node;
  scl_error_t err = create_node(alloc, &node, element, list->element_size);
  if (err != SCL_OK)
    return err;

  scl_spinlock_lock(&list->lock);
  node->prev = list->tail;
  if (list->tail)
    list->tail->next = node;
  else
    list->head = node;
  list->tail = node;
  atomic_fetch_add_explicit(&list->count, 1, memory_order_relaxed);
  scl_spinlock_unlock(&list->lock);
  return SCL_OK;
}

scl_error_t scl_cdlist_pop_front(scl_allocator_t *alloc,
                                 scl_concurrent_dlist_t *list,
                                 void *SCL_RESTRICT out) {
  if (scl_unlikely(!list || !out))
    return SCL_ERR_NULL_PTR;
  scl_spinlock_lock(&list->lock);
  if (scl_unlikely(!list->head)) {
    scl_spinlock_unlock(&list->lock);
    return SCL_ERR_EMPTY;
  }
  scl_concurrent_dlist_node_t *node = list->head;
  scl_memcpy(out, node->data, list->element_size);
  list->head = node->next;
  if (list->head)
    list->head->prev = NULL;
  else
    list->tail = NULL;
  atomic_fetch_sub_explicit(&list->count, 1, memory_order_relaxed);
  scl_spinlock_unlock(&list->lock);
  scl_free(alloc, node);
  return SCL_OK;
}

scl_error_t scl_cdlist_pop_back(scl_allocator_t *alloc,
                                scl_concurrent_dlist_t *list,
                                void *SCL_RESTRICT out) {
  if (scl_unlikely(!list || !out))
    return SCL_ERR_NULL_PTR;
  scl_spinlock_lock(&list->lock);
  if (scl_unlikely(!list->tail)) {
    scl_spinlock_unlock(&list->lock);
    return SCL_ERR_EMPTY;
  }
  scl_concurrent_dlist_node_t *node = list->tail;
  scl_memcpy(out, node->data, list->element_size);
  list->tail = node->prev;
  if (list->tail)
    list->tail->next = NULL;
  else
    list->head = NULL;
  atomic_fetch_sub_explicit(&list->count, 1, memory_order_relaxed);
  scl_spinlock_unlock(&list->lock);
  scl_free(alloc, node);
  return SCL_OK;
}

scl_error_t scl_cdlist_insert_at(scl_allocator_t *alloc,
                                 scl_concurrent_dlist_t *list, size_t index,
                                 const void *SCL_RESTRICT element) {
  if (scl_unlikely(!list || !element))
    return SCL_ERR_NULL_PTR;
  scl_concurrent_dlist_node_t *node;
  scl_error_t err = create_node(alloc, &node, element, list->element_size);
  if (err != SCL_OK)
    return err;

  scl_spinlock_lock(&list->lock);
  size_t cnt = atomic_load_explicit(&list->count, memory_order_relaxed);
  if (index > cnt) {
    scl_spinlock_unlock(&list->lock);
    scl_free(alloc, node);
    return SCL_ERR_INVALID_INDEX;
  }
  if (index == 0) {
    node->next = list->head;
    if (list->head)
      list->head->prev = node;
    else
      list->tail = node;
    list->head = node;
  } else if (index == cnt) {
    node->prev = list->tail;
    if (list->tail)
      list->tail->next = node;
    else
      list->head = node;
    list->tail = node;
  } else {
    scl_concurrent_dlist_node_t *cur = list->head;
    for (size_t i = 0; i < index; i++)
      cur = cur->next;
    node->prev = cur->prev;
    node->next = cur;
    cur->prev->next = node;
    cur->prev = node;
  }
  atomic_fetch_add_explicit(&list->count, 1, memory_order_relaxed);
  scl_spinlock_unlock(&list->lock);
  return SCL_OK;
}

scl_error_t scl_cdlist_remove_at(scl_allocator_t *alloc,
                                 scl_concurrent_dlist_t *list, size_t index,
                                 void *SCL_RESTRICT out) {
  if (scl_unlikely(!list || !out))
    return SCL_ERR_NULL_PTR;
  scl_spinlock_lock(&list->lock);
  size_t cnt = atomic_load_explicit(&list->count, memory_order_relaxed);
  if (index >= cnt) {
    scl_spinlock_unlock(&list->lock);
    return SCL_ERR_INVALID_INDEX;
  }
  scl_concurrent_dlist_node_t *cur;
  if (index == 0) {
    cur = list->head;
    list->head = cur->next;
    if (list->head)
      list->head->prev = NULL;
    else
      list->tail = NULL;
  } else if (index == cnt - 1) {
    cur = list->tail;
    list->tail = cur->prev;
    if (list->tail)
      list->tail->next = NULL;
    else
      list->head = NULL;
  } else {
    cur = list->head;
    for (size_t i = 0; i < index; i++)
      cur = cur->next;
    cur->prev->next = cur->next;
    cur->next->prev = cur->prev;
  }
  scl_memcpy(out, cur->data, list->element_size);
  atomic_fetch_sub_explicit(&list->count, 1, memory_order_relaxed);
  scl_spinlock_unlock(&list->lock);
  scl_free(alloc, cur);
  return SCL_OK;
}

size_t scl_cdlist_count(const scl_concurrent_dlist_t *list) {
  return list ? atomic_load_explicit(&list->count, memory_order_relaxed) : 0;
}

bool scl_cdlist_empty(const scl_concurrent_dlist_t *list) {
  return list ? atomic_load_explicit(&list->count, memory_order_relaxed) == 0
              : true;
}