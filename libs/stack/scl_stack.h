#ifndef SCL_STACK_H
#define SCL_STACK_H

#include "scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    size_t count;
} scl_stack_t;

scl_error_t scl_stack_init(scl_allocator_t *alloc, scl_stack_t *stack, size_t element_size, size_t initial_capacity) SCL_WARN_UNUSED;
void        scl_stack_destroy(scl_allocator_t *alloc, scl_stack_t *stack);
scl_error_t scl_stack_push(scl_allocator_t *alloc, scl_stack_t *stack, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_stack_pop(scl_stack_t *stack, void *out) SCL_WARN_UNUSED;
scl_error_t scl_stack_peek(const scl_stack_t *stack, void *out) SCL_WARN_UNUSED;
size_t      scl_stack_count(const scl_stack_t *stack);
bool        scl_stack_empty(const scl_stack_t *stack);

scl_error_t scl_stack_search(const scl_stack_t *stack, const void *key,
                             int (*cmp)(const void *, const void *),
                             size_t *out_index) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
