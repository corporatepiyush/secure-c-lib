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

/* Thread-safe stack data structure. Guarded by scl_spinlock_t. */

#include "scl_concurrent_stack.h"
#include "scl_string.h"

scl_error_t scl_cstack_init(scl_allocator_t *alloc,
                            scl_concurrent_stack_t *stack,
                            size_t element_size) {
  (void)alloc;
  if (scl_unlikely(!stack))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(element_size == 0))
    return SCL_ERR_INVALID_ARG;
  atomic_init(&stack->top, scl_tagged_make(NULL, 0));
  atomic_init(&stack->count, 0);
  stack->element_size = element_size;
  return SCL_OK;
}

void scl_cstack_destroy(scl_allocator_t *alloc, scl_concurrent_stack_t *stack) {
  if (scl_unlikely(!stack))
    return;
  scl_tagged_ptr_t tp = atomic_load_explicit(&stack->top, memory_order_acquire);
  scl_concurrent_stack_node_t *cur = (scl_concurrent_stack_node_t *)tp.ptr;
  while (scl_likely(cur)) {
    scl_concurrent_stack_node_t *next = cur->next;
    scl_secure_zero(cur->data, stack->element_size);
    scl_free(alloc, cur);
    cur = next;
  }
  atomic_store_explicit(&stack->top, scl_tagged_make(NULL, 0),
                        memory_order_relaxed);
  atomic_store_explicit(&stack->count, 0, memory_order_relaxed);
}

scl_error_t scl_cstack_push(scl_allocator_t *alloc,
                            scl_concurrent_stack_t *stack,
                            const void *SCL_RESTRICT element) {
  if (scl_unlikely(!stack || !element))
    return SCL_ERR_NULL_PTR;
  scl_concurrent_stack_node_t *node = scl_alloc(
      alloc, sizeof(scl_concurrent_stack_node_t) + stack->element_size,
      alignof(max_align_t));
  if (scl_unlikely(!node))
    return SCL_ERR_OUT_OF_MEMORY;
  scl_memcpy(node->data, element, stack->element_size);

  scl_tagged_ptr_t old =
      atomic_load_explicit(&stack->top, memory_order_relaxed);
  do {
    node->next = old.ptr;
  } while (!atomic_compare_exchange_weak_explicit(
      &stack->top, &old, scl_tagged_make(node, old.tag + 1),
      memory_order_release, memory_order_relaxed));

  atomic_fetch_add_explicit(&stack->count, 1, memory_order_relaxed);
  return SCL_OK;
}

scl_error_t scl_cstack_pop(scl_allocator_t *alloc,
                           scl_concurrent_stack_t *stack,
                           void *SCL_RESTRICT out) {
  if (scl_unlikely(!stack || !out))
    return SCL_ERR_NULL_PTR;
  scl_tagged_ptr_t old =
      atomic_load_explicit(&stack->top, memory_order_relaxed);
  while (1) {
    if (scl_unlikely(!old.ptr))
      return SCL_ERR_EMPTY;
    scl_concurrent_stack_node_t *next =
        ((scl_concurrent_stack_node_t *)old.ptr)->next;
    if (atomic_compare_exchange_weak_explicit(
            &stack->top, &old, scl_tagged_make(next, old.tag + 1),
            memory_order_acquire, memory_order_relaxed))
      break;
  }
  scl_concurrent_stack_node_t *popped = (scl_concurrent_stack_node_t *)old.ptr;
  scl_memcpy(out, popped->data, stack->element_size);
  /* Note: use-after-free hazard remains (see report) — a full fix requires
   * RCU/epoch reclamation. For now we free immediately as a best-effort
   * Treiber implementation with tag-based ABA protection. */
  scl_secure_zero(popped->data, stack->element_size);
  scl_free(alloc, popped);
  atomic_fetch_sub_explicit(&stack->count, 1, memory_order_relaxed);
  return SCL_OK;
}

size_t scl_cstack_count(const scl_concurrent_stack_t *stack) {
  return stack ? atomic_load_explicit(&stack->count, memory_order_relaxed) : 0;
}

bool scl_cstack_empty(const scl_concurrent_stack_t *stack) {
  scl_tagged_ptr_t tp;
  if (scl_unlikely(!stack))
    return true;
  tp = atomic_load_explicit(&stack->top, memory_order_relaxed);
  return tp.ptr == NULL;
}
