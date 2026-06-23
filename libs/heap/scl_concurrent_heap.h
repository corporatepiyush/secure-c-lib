#ifndef SCL_CONCURRENT_HEAP_H
#define SCL_CONCURRENT_HEAP_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    atomic_size_t count;
    scl_cmp_func_t cmp;
    scl_spinlock_t lock;
} scl_concurrent_heap_t;

scl_error_t scl_cheap_init(scl_allocator_t *alloc, scl_concurrent_heap_t *heap, size_t element_size,
                          size_t capacity, scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_cheap_destroy(scl_allocator_t *alloc, scl_concurrent_heap_t *heap);
scl_error_t scl_cheap_push(scl_allocator_t *alloc, scl_concurrent_heap_t *heap, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_cheap_pop(scl_concurrent_heap_t *heap, void *out) SCL_WARN_UNUSED;
scl_error_t scl_cheap_peek(scl_concurrent_heap_t *heap, void *out) SCL_WARN_UNUSED;
size_t      scl_cheap_count(const scl_concurrent_heap_t *heap);
bool        scl_cheap_empty(const scl_concurrent_heap_t *heap);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
