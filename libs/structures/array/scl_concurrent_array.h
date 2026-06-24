#ifndef SCL_CONCURRENT_ARRAY_H
#define SCL_CONCURRENT_ARRAY_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    atomic_size_t count SCL_CACHE_ALIGNED;
} scl_concurrent_array_t;

scl_error_t scl_carray_init(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_array_t *SCL_RESTRICT arr, size_t element_size, size_t capacity) SCL_WARN_UNUSED;
void        scl_carray_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_array_t *SCL_RESTRICT arr);
scl_error_t scl_carray_push(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_array_t *SCL_RESTRICT arr, const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_carray_pop(scl_concurrent_array_t *SCL_RESTRICT arr, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_carray_get(const scl_concurrent_array_t *SCL_RESTRICT arr, size_t index, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_carray_set(scl_concurrent_array_t *SCL_RESTRICT arr, size_t index, const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
SCL_PURE size_t      scl_carray_count(const scl_concurrent_array_t *SCL_RESTRICT arr);
SCL_PURE bool        scl_carray_empty(const scl_concurrent_array_t *SCL_RESTRICT arr);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
