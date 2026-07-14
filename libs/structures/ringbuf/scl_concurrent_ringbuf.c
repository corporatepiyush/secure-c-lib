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

/* Thread-safe ringbuf data structure. Guarded by scl_spinlock_t. */

#include "scl_concurrent_ringbuf.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_cringbuf_init(scl_allocator_t *alloc,
                              scl_concurrent_ringbuf_t *rb, size_t element_size,
                              size_t capacity) {
  if (scl_unlikely(!rb))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(element_size == 0 || capacity == 0))
    return SCL_ERR_INVALID_ARG;
  size_t cap = scl_bit_ceil_sz(capacity);
  size_t bytes;
  if (scl_mul_overflow(cap, element_size, &bytes))
    return SCL_ERR_SIZE_OVERFLOW;
  rb->data = scl_alloc(alloc, bytes, alignof(max_align_t));
  if (scl_unlikely(!rb->data))
    return SCL_ERR_OUT_OF_MEMORY;
  rb->element_size = element_size;
  rb->capacity = cap;
  rb->mask = cap - 1;
  atomic_init(&rb->head, 0);
  atomic_init(&rb->count, 0);
  scl_spinlock_init(&rb->lock);
  return SCL_OK;
}

void scl_cringbuf_destroy(scl_allocator_t *alloc,
                          scl_concurrent_ringbuf_t *rb) {
  if (scl_unlikely(!rb))
    return;
  scl_free(alloc, rb->data);
  rb->data = NULL;
  rb->capacity = 0;
  atomic_store_explicit(&rb->head, 0, memory_order_relaxed);
  atomic_store_explicit(&rb->count, 0, memory_order_relaxed);
}

scl_error_t scl_cringbuf_push(scl_concurrent_ringbuf_t *rb,
                              const void *element) {
  if (scl_unlikely(!rb || !element))
    return SCL_ERR_NULL_PTR;
  scl_spinlock_lock(&rb->lock);
  size_t cnt = atomic_load_explicit(&rb->count, memory_order_relaxed);
  if (scl_unlikely(cnt == rb->capacity)) {
    scl_spinlock_unlock(&rb->lock);
    return SCL_ERR_FULL;
  }

  size_t head = atomic_load_explicit(&rb->head, memory_order_acquire);
  size_t es = rb->element_size;
  size_t idx = (head + cnt) & rb->mask;
  scl_memcpy(rb->data + idx * es, element, es);
  atomic_thread_fence(memory_order_release);
  atomic_store_explicit(&rb->count, cnt + 1, memory_order_release);
  scl_spinlock_unlock(&rb->lock);
  return SCL_OK;
}

scl_error_t scl_cringbuf_pop(scl_concurrent_ringbuf_t *rb, void *out) {
  if (scl_unlikely(!rb || !out))
    return SCL_ERR_NULL_PTR;
  scl_spinlock_lock(&rb->lock);
  size_t cnt = atomic_load_explicit(&rb->count, memory_order_acquire);
  if (scl_unlikely(cnt == 0)) {
    scl_spinlock_unlock(&rb->lock);
    return SCL_ERR_EMPTY;
  }

  size_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);
  size_t es = rb->element_size;
  scl_memcpy(out, rb->data + head * es, es);
  atomic_thread_fence(memory_order_release);
  atomic_store_explicit(&rb->head, (head + 1) & rb->mask, memory_order_release);
  atomic_store_explicit(&rb->count, cnt - 1, memory_order_release);
  scl_spinlock_unlock(&rb->lock);
  return SCL_OK;
}

scl_error_t scl_cringbuf_peek(const scl_concurrent_ringbuf_t *rb, size_t index,
                              void *SCL_RESTRICT out) {
  if (scl_unlikely(!rb || !out))
    return SCL_ERR_NULL_PTR;
  size_t cnt = atomic_load_explicit(&rb->count, memory_order_acquire);
  if (scl_unlikely(index >= cnt))
    return SCL_ERR_INVALID_INDEX;

  size_t head = atomic_load_explicit(&rb->head, memory_order_acquire);
  size_t pos = (head + index) & rb->mask;
  scl_memcpy(out, rb->data + pos * rb->element_size, rb->element_size);
  return SCL_OK;
}

size_t scl_cringbuf_count(const scl_concurrent_ringbuf_t *rb) {
  return rb ? atomic_load_explicit(&rb->count, memory_order_acquire) : 0;
}

bool scl_cringbuf_empty(const scl_concurrent_ringbuf_t *rb) {
  return rb ? atomic_load_explicit(&rb->count, memory_order_acquire) == 0
            : true;
}
