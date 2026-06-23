#ifndef SCL_SORT_H
#define SCL_SORT_H

#include "scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

scl_error_t scl_sort_quick(void *base, size_t count, size_t element_size,
                           scl_cmp_func_t cmp) SCL_WARN_UNUSED;
scl_error_t scl_sort_merge(scl_allocator_t *alloc, void *base, size_t count, size_t element_size,
                           scl_cmp_func_t cmp) SCL_WARN_UNUSED;
scl_error_t scl_sort_heap(void *base, size_t count, size_t element_size,
                          scl_cmp_func_t cmp) SCL_WARN_UNUSED;
scl_error_t scl_sort_insertion(void *base, size_t count, size_t element_size,
                               scl_cmp_func_t cmp) SCL_WARN_UNUSED;
scl_error_t scl_sort_selection(void *base, size_t count, size_t element_size,
                               scl_cmp_func_t cmp) SCL_WARN_UNUSED;
scl_error_t scl_sort_bubble(void *base, size_t count, size_t element_size,
                            scl_cmp_func_t cmp) SCL_WARN_UNUSED;
scl_error_t scl_sort_shell(void *base, size_t count, size_t element_size,
                           scl_cmp_func_t cmp) SCL_WARN_UNUSED;
scl_error_t scl_sort_counting(scl_allocator_t *alloc, int32_t *base, size_t count) SCL_WARN_UNUSED;
scl_error_t scl_sort_radix(scl_allocator_t *alloc, int32_t *base, size_t count) SCL_WARN_UNUSED;
scl_error_t scl_sort_bucket(scl_allocator_t *alloc, void *base, size_t count, size_t element_size,
                            scl_cmp_func_t cmp) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
