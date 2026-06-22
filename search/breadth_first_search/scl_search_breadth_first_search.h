#ifndef SCL_SEARCH_BREADTH_FIRST_SEARCH_H
#define SCL_SEARCH_BREADTH_FIRST_SEARCH_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../../common/scl_common.h"
#include <stdbool.h>

scl_error_t scl_search_breadth_first_search(const scl_graph_t *graph, int start, bool *visited) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
