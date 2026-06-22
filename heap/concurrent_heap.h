#ifndef SCL_CONCURRENT_HEAP_H
#define SCL_CONCURRENT_HEAP_H

#include "../common/scl_common.h"
#include <stdatomic.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef int (*scl_concurrent_cmp_func_t)(const void *a, const void *b);

typedef struct {
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    atomic_size_t count;
    scl_concurrent_cmp_func_t cmp;
    atomic_flag lock;
} scl_concurrent_heap_t;

scl_error_t scl_concurrent_heap_init(scl_concurrent_heap_t *heap, size_t element_size,
                                     size_t capacity, scl_concurrent_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_concurrent_heap_destroy(scl_concurrent_heap_t *heap);
scl_error_t scl_concurrent_heap_push(scl_concurrent_heap_t *heap, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_heap_pop(scl_concurrent_heap_t *heap, void *out) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_heap_peek(scl_concurrent_heap_t *heap, void *out) SCL_WARN_UNUSED;
size_t      scl_concurrent_heap_count(const scl_concurrent_heap_t *heap);
bool        scl_concurrent_heap_empty(const scl_concurrent_heap_t *heap);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
