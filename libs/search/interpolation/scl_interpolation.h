#ifndef SCL_SEARCH_INTERPOLATION_SEARCH_H
#define SCL_SEARCH_INTERPOLATION_SEARCH_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"
#include <stdint.h>

scl_error_t scl_search_interpolation_search(const int64_t * arr, size_t count, int64_t key, size_t * out_index) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
