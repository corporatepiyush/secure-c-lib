#ifndef SCL_SORT_RADIX_SORT_H
#define SCL_SORT_RADIX_SORT_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../../common/scl_common.h"

scl_error_t scl_sort_radix_sort(scl_allocator_t *alloc, int32_t *base, size_t count) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
