#include "concurrent_stack.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_atomic_stack_init(scl_allocator_t *alloc, scl_atomic_stack_t *stack, size_t element_size)
{
    (void)alloc;
    if (!stack) return SCL_ERR_NULL_PTR;
    if (element_size == 0) return SCL_ERR_INVALID_ARG;
    atomic_init(&stack->top, (uintptr_t)NULL);
    atomic_init(&stack->count, 0);
    stack->element_size = element_size;
    return SCL_OK;
}

void scl_atomic_stack_destroy(scl_allocator_t *alloc, scl_atomic_stack_t *stack)
{
    if (!stack) return;
    scl_atomic_stack_node_t *cur = (scl_atomic_stack_node_t *)atomic_load_explicit(&stack->top, memory_order_acquire);
    while (cur) {
        scl_atomic_stack_node_t *next = cur->next;
        scl_free(alloc, cur->data);
        scl_free(alloc, cur);
        cur = next;
    }
    atomic_store_explicit(&stack->top, (uintptr_t)NULL, memory_order_relaxed);
    atomic_store_explicit(&stack->count, 0, memory_order_relaxed);
}

scl_error_t scl_atomic_stack_push(scl_allocator_t *alloc, scl_atomic_stack_t *stack, const void *element)
{
    if (!stack || !element) return SCL_ERR_NULL_PTR;
    scl_atomic_stack_node_t *node = scl_alloc(alloc, sizeof(scl_atomic_stack_node_t), alignof(max_align_t));
    if (!node) return SCL_ERR_OUT_OF_MEMORY;
    node->data = scl_alloc(alloc, stack->element_size, alignof(max_align_t));
    if (!node->data) {
        scl_free(alloc, node);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    memcpy(node->data, element, stack->element_size);

    scl_atomic_stack_node_t *old_top = (scl_atomic_stack_node_t *)atomic_load_explicit(&stack->top, memory_order_relaxed);
    do {
        node->next = old_top;
    } while (!atomic_compare_exchange_weak_explicit(&stack->top,
             (uintptr_t *)&old_top, (uintptr_t)node,
             memory_order_release, memory_order_relaxed));

    atomic_fetch_add_explicit(&stack->count, 1, memory_order_relaxed);
    return SCL_OK;
}

scl_error_t scl_atomic_stack_pop(scl_allocator_t *alloc, scl_atomic_stack_t *stack, void *out)
{
    if (!stack || !out) return SCL_ERR_NULL_PTR;
    scl_atomic_stack_node_t *old_top = (scl_atomic_stack_node_t *)atomic_load_explicit(&stack->top, memory_order_relaxed);
    while (1) {
        if (!old_top) return SCL_ERR_EMPTY;
        if (atomic_compare_exchange_weak_explicit(&stack->top,
                (uintptr_t *)&old_top, (uintptr_t)old_top->next,
                memory_order_acquire, memory_order_relaxed))
            break;
    }
    memcpy(out, old_top->data, stack->element_size);
    scl_free(alloc, old_top->data);
    scl_free(alloc, old_top);
    atomic_fetch_sub_explicit(&stack->count, 1, memory_order_relaxed);
    return SCL_OK;
}

size_t scl_atomic_stack_count(const scl_atomic_stack_t *stack)
{
    return stack ? atomic_load_explicit(&stack->count, memory_order_relaxed) : 0;
}

bool scl_atomic_stack_empty(const scl_atomic_stack_t *stack)
{
    return stack ? (atomic_load_explicit(&stack->top, memory_order_relaxed) == 0) : true;
}
