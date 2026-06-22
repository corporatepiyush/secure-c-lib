#ifndef SCL_CONCURRENT_ARRAY_H
#define SCL_CONCURRENT_ARRAY_H

#include "../common/scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    atomic_size_t count;
} scl_atomic_array_t;

scl_error_t scl_atomic_array_init(scl_allocator_t *alloc, scl_atomic_array_t *arr, size_t element_size, size_t capacity) SCL_WARN_UNUSED;
void        scl_atomic_array_destroy(scl_allocator_t *alloc, scl_atomic_array_t *arr);
scl_error_t scl_atomic_array_push(scl_allocator_t *alloc, scl_atomic_array_t *arr, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_atomic_array_pop(scl_atomic_array_t *arr, void *out) SCL_WARN_UNUSED;
scl_error_t scl_atomic_array_get(const scl_atomic_array_t *arr, size_t index, void *out) SCL_WARN_UNUSED;
scl_error_t scl_atomic_array_set(scl_atomic_array_t *arr, size_t index, const void *element) SCL_WARN_UNUSED;
size_t      scl_atomic_array_count(const scl_atomic_array_t *arr);
bool        scl_atomic_array_empty(const scl_atomic_array_t *arr);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
