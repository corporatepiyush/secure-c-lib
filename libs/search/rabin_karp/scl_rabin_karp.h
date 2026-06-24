#ifndef SCL_SEARCH_RABIN_KARP_H
#define SCL_SEARCH_RABIN_KARP_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

scl_error_t scl_search_rabin_karp(const char * text, size_t tlen, const char * pat, size_t plen, size_t * pos) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
