#ifndef SCL_SORT_INSERTION_SORT_H
#define SCL_SORT_INSERTION_SORT_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

scl_error_t scl_sort_insertion_sort(void *base, size_t count, size_t element_size, scl_cmp_func_t cmp) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
