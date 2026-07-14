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

/* Thread-safe deque data structure. Guarded by scl_spinlock_t. */

#include "scl_concurrent_deque.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_cdeque_init(scl_allocator_t *alloc,
                            scl_concurrent_deque_t *deque, size_t element_size,
                            size_t capacity) {
  if (scl_unlikely(!deque))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(element_size == 0 || capacity == 0))
    return SCL_ERR_INVALID_ARG;
  size_t cap = capacity < 4 ? 4 : scl_bit_ceil_sz(capacity);
  size_t bytes;
  if (scl_mul_overflow(cap, element_size, &bytes))
    return SCL_ERR_SIZE_OVERFLOW;
  deque->data = scl_alloc(alloc, bytes, alignof(max_align_t));
  if (scl_unlikely(!deque->data))
    return SCL_ERR_OUT_OF_MEMORY;
  deque->element_size = element_size;
  deque->capacity = cap;
  deque->mask = cap - 1;
  deque->head = 0;
  atomic_init(&deque->count, 0);
  scl_spinlock_init(&deque->lock);
  return SCL_OK;
}

void scl_cdeque_destroy(scl_allocator_t *alloc, scl_concurrent_deque_t *deque) {
  if (scl_unlikely(!deque))
    return;
  scl_free(alloc, deque->data);
  deque->data = NULL;
  deque->capacity = 0;
  deque->mask = 0;
  atomic_store_explicit(&deque->count, 0, memory_order_relaxed);
}

scl_error_t scl_cdeque_push_front(scl_allocator_t *alloc,
                                  scl_concurrent_deque_t *deque,
                                  const void *SCL_RESTRICT element) {
  (void)alloc;
  if (scl_unlikely(!deque || !element))
    return SCL_ERR_NULL_PTR;
  scl_spinlock_lock(&deque->lock);
  size_t cnt = atomic_load_explicit(&deque->count, memory_order_relaxed);
  if (cnt == deque->capacity) {
    scl_spinlock_unlock(&deque->lock);
    return SCL_ERR_FULL;
  }
  deque->head = (deque->head - 1) & deque->mask;
  size_t offset;
  if (scl_mul_overflow(deque->head, deque->element_size, &offset)) {
    scl_spinlock_unlock(&deque->lock);
    return SCL_ERR_SIZE_OVERFLOW;
  }
  scl_memcpy(deque->data + offset, element, deque->element_size);
  atomic_fetch_add_explicit(&deque->count, 1, memory_order_relaxed);
  scl_spinlock_unlock(&deque->lock);
  return SCL_OK;
}

scl_error_t scl_cdeque_push_back(scl_allocator_t *alloc,
                                 scl_concurrent_deque_t *deque,
                                 const void *SCL_RESTRICT element) {
  (void)alloc;
  if (scl_unlikely(!deque || !element))
    return SCL_ERR_NULL_PTR;
  scl_spinlock_lock(&deque->lock);
  size_t cnt = atomic_load_explicit(&deque->count, memory_order_relaxed);
  if (cnt == deque->capacity) {
    scl_spinlock_unlock(&deque->lock);
    return SCL_ERR_FULL;
  }
  size_t tail = (deque->head + cnt) & deque->mask;
  size_t offset;
  if (scl_mul_overflow(tail, deque->element_size, &offset)) {
    scl_spinlock_unlock(&deque->lock);
    return SCL_ERR_SIZE_OVERFLOW;
  }
  scl_memcpy(deque->data + offset, element, deque->element_size);
  atomic_fetch_add_explicit(&deque->count, 1, memory_order_relaxed);
  scl_spinlock_unlock(&deque->lock);
  return SCL_OK;
}

scl_error_t scl_cdeque_pop_front(scl_concurrent_deque_t *deque, void *out) {
  if (scl_unlikely(!deque || !out))
    return SCL_ERR_NULL_PTR;
  scl_spinlock_lock(&deque->lock);
  size_t cnt = atomic_load_explicit(&deque->count, memory_order_relaxed);
  if (cnt == 0) {
    scl_spinlock_unlock(&deque->lock);
    return SCL_ERR_EMPTY;
  }
  size_t offset;
  if (scl_mul_overflow(deque->head, deque->element_size, &offset)) {
    scl_spinlock_unlock(&deque->lock);
    return SCL_ERR_SIZE_OVERFLOW;
  }
  scl_memcpy(out, deque->data + offset, deque->element_size);
  deque->head = (deque->head + 1) & deque->mask;
  atomic_fetch_sub_explicit(&deque->count, 1, memory_order_relaxed);
  scl_spinlock_unlock(&deque->lock);
  return SCL_OK;
}

scl_error_t scl_cdeque_pop_back(scl_concurrent_deque_t *deque, void *out) {
  if (scl_unlikely(!deque || !out))
    return SCL_ERR_NULL_PTR;
  scl_spinlock_lock(&deque->lock);
  size_t cnt = atomic_load_explicit(&deque->count, memory_order_relaxed);
  if (cnt == 0) {
    scl_spinlock_unlock(&deque->lock);
    return SCL_ERR_EMPTY;
  }
  size_t tail = (deque->head + cnt - 1) & deque->mask;
  size_t offset;
  if (scl_mul_overflow(tail, deque->element_size, &offset)) {
    scl_spinlock_unlock(&deque->lock);
    return SCL_ERR_SIZE_OVERFLOW;
  }
  scl_memcpy(out, deque->data + offset, deque->element_size);
  atomic_fetch_sub_explicit(&deque->count, 1, memory_order_relaxed);
  scl_spinlock_unlock(&deque->lock);
  return SCL_OK;
}

size_t scl_cdeque_count(const scl_concurrent_deque_t *deque) {
  return deque ? atomic_load_explicit(&deque->count, memory_order_relaxed) : 0;
}

bool scl_cdeque_empty(const scl_concurrent_deque_t *deque) {
  return deque ? atomic_load_explicit(&deque->count, memory_order_relaxed) == 0
               : true;
}
