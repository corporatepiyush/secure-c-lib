#ifndef SCL_HEAP_H
#define SCL_HEAP_H

#include "../common/scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef int (*scl_cmp_func_t)(const void *a, const void *b);

typedef struct {
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    size_t count;
    scl_cmp_func_t cmp;
} scl_heap_t;

scl_error_t scl_heap_init(scl_heap_t *heap, size_t element_size, size_t initial_capacity,
                          scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_heap_destroy(scl_heap_t *heap);
scl_error_t scl_heap_push(scl_heap_t *heap, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_heap_pop(scl_heap_t *heap, void *out) SCL_WARN_UNUSED;
scl_error_t scl_heap_peek(const scl_heap_t *heap, void *out) SCL_WARN_UNUSED;
size_t      scl_heap_count(const scl_heap_t *heap);
bool        scl_heap_empty(const scl_heap_t *heap);

scl_error_t scl_heap_search(const scl_heap_t *heap, const void *key,
                            scl_cmp_func_t cmp,
                            size_t *out_index) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
