#ifndef SCL_SEARCH_A_STAR_H
#define SCL_SEARCH_A_STAR_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

scl_error_t scl_search_a_star(scl_allocator_t *alloc, int sx, int sy, int gx, int gy, int **restrict grid, int w, int h, int *restrict px, int *restrict py, size_t *restrict plen, size_t maxplen) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
