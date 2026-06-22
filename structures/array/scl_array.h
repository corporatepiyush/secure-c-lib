#ifndef SCL_ARRAY_H
#define SCL_ARRAY_H

#include "../common/scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    size_t count;
} scl_array_t;

scl_error_t scl_array_init(scl_allocator_t *alloc, scl_array_t *arr, size_t element_size, size_t initial_capacity) SCL_WARN_UNUSED;
void        scl_array_destroy(scl_allocator_t *alloc, scl_array_t *arr);
scl_error_t scl_array_push(scl_allocator_t *alloc, scl_array_t *arr, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_array_pop(scl_array_t *arr, void *out) SCL_WARN_UNUSED;
scl_error_t scl_array_get(const scl_array_t *arr, size_t index, void *out) SCL_WARN_UNUSED;
scl_error_t scl_array_set(scl_array_t *arr, size_t index, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_array_insert(scl_allocator_t *alloc, scl_array_t *arr, size_t index, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_array_remove(scl_array_t *arr, size_t index, void *out) SCL_WARN_UNUSED;
scl_error_t scl_array_reserve(scl_allocator_t *alloc, scl_array_t *arr, size_t new_capacity) SCL_WARN_UNUSED;
scl_error_t scl_array_shrink(scl_allocator_t *alloc, scl_array_t *arr) SCL_WARN_UNUSED;
size_t      scl_array_count(const scl_array_t *arr);
size_t      scl_array_capacity(const scl_array_t *arr);
bool        scl_array_empty(const scl_array_t *arr);

scl_error_t scl_array_linear_search(const scl_array_t *arr, const void *key,
                                    int (*cmp)(const void *, const void *),
                                    size_t *out_index) SCL_WARN_UNUSED;
scl_error_t scl_array_binary_search(const scl_array_t *arr, const void *key,
                                    int (*cmp)(const void *, const void *),
                                    size_t *out_index) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
