#ifndef SCL_CONCURRENT_STACK_H
#define SCL_CONCURRENT_STACK_H

#include "../common/scl_common.h"
#include <stdatomic.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_concurrent_stack_node {
    void *data;
    struct scl_concurrent_stack_node *next;
} scl_concurrent_stack_node_t;

typedef struct {
    atomic_uintptr_t top;
    size_t element_size;
    atomic_size_t count;
} scl_concurrent_stack_t;

scl_error_t scl_concurrent_stack_init(scl_concurrent_stack_t *stack, size_t element_size) SCL_WARN_UNUSED;
void        scl_concurrent_stack_destroy(scl_concurrent_stack_t *stack);
scl_error_t scl_concurrent_stack_push(scl_concurrent_stack_t *stack, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_stack_pop(scl_concurrent_stack_t *stack, void *out) SCL_WARN_UNUSED;
size_t      scl_concurrent_stack_count(const scl_concurrent_stack_t *stack);
bool        scl_concurrent_stack_empty(const scl_concurrent_stack_t *stack);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
