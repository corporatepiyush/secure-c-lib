#ifndef SCL_SEARCH_LINEAR_SEARCH_H
#define SCL_SEARCH_LINEAR_SEARCH_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

scl_error_t scl_search_linear_search(const void * base, size_t count, size_t elem_size, const void * key, scl_cmp_func_t cmp, size_t * out_index) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
