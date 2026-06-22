#ifndef SCL_SEARCH_UNBOUNDED_BINARY_SEARCH_H
#define SCL_SEARCH_UNBOUNDED_BINARY_SEARCH_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../../common/scl_common.h"

scl_error_t scl_search_unbounded_binary_search(scl_cmp_func_t cmp, const void *restrict key, size_t *restrict out_index, void *(*getter)(size_t index, void *ctx), void *ctx, size_t max_count) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
