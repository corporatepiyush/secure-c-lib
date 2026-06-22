#ifndef SCL_CONCURRENT_ARRAY_H
#define SCL_CONCURRENT_ARRAY_H

#include "../common/scl_common.h"
#include <stdatomic.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    atomic_size_t count;
} scl_concurrent_array_t;

scl_error_t scl_concurrent_array_init(scl_concurrent_array_t *arr, size_t element_size, size_t capacity) SCL_WARN_UNUSED;
void        scl_concurrent_array_destroy(scl_concurrent_array_t *arr);
scl_error_t scl_concurrent_array_push_back(scl_concurrent_array_t *arr, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_array_pop_back(scl_concurrent_array_t *arr, void *out) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_array_get(const scl_concurrent_array_t *arr, size_t index, void *out) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_array_set(scl_concurrent_array_t *arr, size_t index, const void *element) SCL_WARN_UNUSED;
size_t      scl_concurrent_array_count(const scl_concurrent_array_t *arr);
bool        scl_concurrent_array_empty(const scl_concurrent_array_t *arr);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
