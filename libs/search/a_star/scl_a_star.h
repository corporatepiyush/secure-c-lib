#ifndef SCL_SEARCH_A_STAR_H
#define SCL_SEARCH_A_STAR_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

scl_error_t scl_search_a_star(scl_allocator_t * alloc, int sx, int sy, int gx, int gy, int **SCL_RESTRICT grid, int w, int h, int * px, int * py, size_t * plen, size_t maxplen) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
