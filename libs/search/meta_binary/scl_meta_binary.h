#ifndef SCL_SEARCH_META_BINARY_SEARCH_H
#define SCL_SEARCH_META_BINARY_SEARCH_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"
#include <stdint.h>

scl_error_t scl_search_meta_binary_search(const int32_t *restrict arr, size_t count, int32_t key, size_t *restrict out_index) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
