#ifndef SCL_SEARCH_DIJKSTRA_H
#define SCL_SEARCH_DIJKSTRA_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"
#include <stdint.h>

scl_error_t scl_search_dijkstra(scl_allocator_t *alloc, const scl_graph_t *graph, int start, int64_t *restrict dist, int *restrict prev) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
