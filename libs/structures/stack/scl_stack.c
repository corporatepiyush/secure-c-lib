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

/* stack data structure. */

#include "scl_stack.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_stack_init(scl_allocator_t *alloc, scl_stack_t *stack,
                           size_t element_size, size_t initial_capacity) {
  if (scl_unlikely(!stack))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(element_size == 0))
    return SCL_ERR_INVALID_ARG;

  stack->data = NULL;
  stack->element_size = element_size;
  stack->capacity = 0;
  stack->count = 0;

  if (initial_capacity > 0) {
    size_t bytes;
    if (scl_mul_overflow(initial_capacity, element_size, &bytes))
      return SCL_ERR_SIZE_OVERFLOW;
    stack->data = scl_alloc(alloc, bytes, alignof(max_align_t));
    if (scl_unlikely(!stack->data))
      return SCL_ERR_OUT_OF_MEMORY;
    stack->capacity = initial_capacity;
  }
  return SCL_OK;
}

void scl_stack_destroy(scl_allocator_t *alloc, scl_stack_t *stack) {
  if (stack) {
    scl_free(alloc, stack->data);
    stack->data = NULL;
    stack->capacity = 0;
    stack->count = 0;
  }
}

scl_error_t scl_stack_push(scl_allocator_t *alloc, scl_stack_t *stack,
                           const void *SCL_RESTRICT element) {
  if (scl_unlikely(!stack || !element))
    return SCL_ERR_NULL_PTR;

  size_t cnt = stack->count;
  size_t es = stack->element_size;

  if (scl_unlikely(cnt == stack->capacity)) {
    size_t new_cap = stack->capacity == 0 ? 4 : stack->capacity * 2;
    size_t old_bytes = stack->capacity * es;
    size_t new_bytes;
    if (scl_mul_overflow(new_cap, es, &new_bytes))
      return SCL_ERR_SIZE_OVERFLOW;
    unsigned char *tmp = scl_realloc(alloc, stack->data, old_bytes, new_bytes,
                                     alignof(max_align_t));
    if (scl_unlikely(!tmp))
      return SCL_ERR_OUT_OF_MEMORY;
    stack->data = tmp;
    stack->capacity = new_cap;
  }

  scl_memcpy(stack->data + cnt * es, element, es);
  stack->count = cnt + 1;
  return SCL_OK;
}

scl_error_t scl_stack_pop(scl_stack_t *stack, void *out) {
  if (scl_unlikely(!stack || !out))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(stack->count == 0))
    return SCL_ERR_EMPTY;

  size_t es = stack->element_size;
  stack->count--;
  scl_memcpy(out, stack->data + stack->count * es, es);
  return SCL_OK;
}

scl_error_t scl_stack_peek(const scl_stack_t *stack, void *out) {
  if (scl_unlikely(!stack || !out))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(stack->count == 0))
    return SCL_ERR_EMPTY;

  scl_memcpy(out, stack->data + (stack->count - 1) * stack->element_size,
             stack->element_size);
  return SCL_OK;
}

size_t scl_stack_count(const scl_stack_t *stack) {
  return stack ? stack->count : 0;
}

bool scl_stack_empty(const scl_stack_t *stack) {
  return stack ? stack->count == 0 : true;
}

scl_error_t scl_stack_search(const scl_stack_t *restrict stack,
                             const void *restrict key,
                             int (*cmp)(const void *, const void *),
                             size_t *restrict out_index) {
  if (scl_unlikely(!stack || !key || !cmp || !out_index))
    return SCL_ERR_NULL_PTR;

  size_t cnt = stack->count;
  size_t es = stack->element_size;
  unsigned char *data = stack->data;

  for (size_t i = 0; i < cnt; i++) {
    if (cmp(data + i * es, key) == 0) {
      *out_index = i;
      return SCL_OK;
    }
  }
  return SCL_ERR_NOT_FOUND;
}
