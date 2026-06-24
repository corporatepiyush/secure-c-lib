#ifndef SCL_SEARCH_FLOYD_WARSHALL_H
#define SCL_SEARCH_FLOYD_WARSHALL_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"
#include <stdint.h>

scl_error_t scl_search_floyd_warshall(int n, const scl_edge_t * edges, size_t ecount, int64_t * dist) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
