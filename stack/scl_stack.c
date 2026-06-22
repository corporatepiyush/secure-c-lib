#include "scl_stack.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_stack_init(scl_allocator_t *alloc, scl_stack_t *stack, size_t element_size, size_t initial_capacity)
{
    if (!stack) return SCL_ERR_NULL_PTR;
    if (element_size == 0) return SCL_ERR_INVALID_ARG;

    stack->data = NULL;
    stack->element_size = element_size;
    stack->capacity = 0;
    stack->count = 0;

    if (initial_capacity > 0) {
        size_t bytes;
        if (scl_mul_overflow(initial_capacity, element_size, &bytes))
            return SCL_ERR_SIZE_OVERFLOW;
        stack->data = scl_alloc(alloc, bytes, alignof(max_align_t));
        if (!stack->data) return SCL_ERR_OUT_OF_MEMORY;
        stack->capacity = initial_capacity;
    }
    return SCL_OK;
}

void scl_stack_destroy(scl_allocator_t *alloc, scl_stack_t *stack)
{
    if (stack) {
        scl_free(alloc, stack->data);
        stack->data = NULL;
        stack->capacity = 0;
        stack->count = 0;
    }
}

scl_error_t scl_stack_push(scl_allocator_t *alloc, scl_stack_t *stack, const void *element)
{
    if (!stack || !element) return SCL_ERR_NULL_PTR;

    if (stack->count == stack->capacity) {
        size_t new_cap = stack->capacity == 0 ? 4 : stack->capacity * 2;
        size_t old_bytes, new_bytes;
        if (scl_mul_overflow(stack->capacity, stack->element_size, &old_bytes))
            old_bytes = 0;
        if (scl_mul_overflow(new_cap, stack->element_size, &new_bytes))
            return SCL_ERR_SIZE_OVERFLOW;
        unsigned char *tmp = scl_realloc(alloc, stack->data, old_bytes, new_bytes, alignof(max_align_t));
        if (!tmp) return SCL_ERR_OUT_OF_MEMORY;
        stack->data = tmp;
        stack->capacity = new_cap;
    }

    size_t offset;
    if (scl_mul_overflow(stack->count, stack->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;

    memcpy(stack->data + offset, element, stack->element_size);
    stack->count++;
    return SCL_OK;
}

scl_error_t scl_stack_pop(scl_stack_t *stack, void *out)
{
    if (!stack || !out) return SCL_ERR_NULL_PTR;
    if (stack->count == 0) return SCL_ERR_EMPTY;

    stack->count--;
    size_t offset;
    if (scl_mul_overflow(stack->count, stack->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;

    memcpy(out, stack->data + offset, stack->element_size);
    return SCL_OK;
}

scl_error_t scl_stack_peek(const scl_stack_t *stack, void *out)
{
    if (!stack || !out) return SCL_ERR_NULL_PTR;
    if (stack->count == 0) return SCL_ERR_EMPTY;

    size_t offset;
    if (scl_mul_overflow(stack->count - 1, stack->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;

    memcpy(out, stack->data + offset, stack->element_size);
    return SCL_OK;
}

size_t scl_stack_count(const scl_stack_t *stack)
{
    return stack ? stack->count : 0;
}

bool scl_stack_empty(const scl_stack_t *stack)
{
    return stack ? stack->count == 0 : true;
}

scl_error_t scl_stack_search(const scl_stack_t *restrict stack, const void *restrict key,
                             int (*cmp)(const void *, const void *),
                             size_t *restrict out_index)
{
    if (__builtin_expect(!stack || !key || !cmp || !out_index, 0))
        return SCL_ERR_NULL_PTR;

    for (size_t i = 0; i < stack->count; i++) {
        if (cmp(stack->data + i * stack->element_size, key) == 0) {
            *out_index = i;
            return SCL_OK;
        }
    }
    return SCL_ERR_NOT_FOUND;
}
