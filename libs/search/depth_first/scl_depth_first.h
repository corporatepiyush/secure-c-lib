#ifndef SCL_SEARCH_DEPTH_FIRST_SEARCH_H
#define SCL_SEARCH_DEPTH_FIRST_SEARCH_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"
#include <stdbool.h>

scl_error_t scl_search_depth_first_search(scl_allocator_t *alloc, const scl_graph_t *graph, int start, bool *visited) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
