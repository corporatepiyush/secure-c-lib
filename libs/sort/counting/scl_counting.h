#ifndef SCL_SORT_COUNTING_SORT_H
#define SCL_SORT_COUNTING_SORT_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

scl_error_t scl_sort_counting_sort(scl_allocator_t *alloc, int32_t *base, size_t count) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
