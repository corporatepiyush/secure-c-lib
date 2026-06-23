#ifndef SCL_SEARCH_QUICKSELECT_H
#define SCL_SEARCH_QUICKSELECT_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

scl_error_t scl_search_quickselect(scl_allocator_t *alloc, void *base, size_t count, size_t elem_size, scl_cmp_func_t cmp, size_t k, void *out) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
