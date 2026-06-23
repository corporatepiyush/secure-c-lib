#ifndef SCL_SEARCH_KMP_H
#define SCL_SEARCH_KMP_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

scl_error_t scl_search_kmp(scl_allocator_t *alloc, const char *restrict text, size_t tlen, const char *restrict pat, size_t plen, size_t *restrict pos) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
