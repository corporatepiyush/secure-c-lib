#ifndef SCL_CONCURRENT_STACK_H
#define SCL_CONCURRENT_STACK_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_concurrent_stack_node {
    void *data;
    struct scl_concurrent_stack_node *next;
} scl_concurrent_stack_node_t;

typedef struct {
    _Atomic scl_tagged_ptr_t top SCL_CACHE_ALIGNED;
    size_t element_size;
    atomic_size_t count SCL_CACHE_ALIGNED;
} scl_concurrent_stack_t;

scl_error_t scl_cstack_init(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_stack_t *SCL_RESTRICT stack, size_t element_size) SCL_WARN_UNUSED;
void        scl_cstack_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_stack_t *SCL_RESTRICT stack);
scl_error_t scl_cstack_push(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_stack_t *SCL_RESTRICT stack, const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_cstack_pop(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_stack_t *SCL_RESTRICT stack, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
SCL_PURE size_t      scl_cstack_count(const scl_concurrent_stack_t *SCL_RESTRICT stack);
SCL_PURE bool        scl_cstack_empty(const scl_concurrent_stack_t *SCL_RESTRICT stack);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
