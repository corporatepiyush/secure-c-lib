#ifndef SCL_SEARCH_EXPONENTIAL_STRING_H
#define SCL_SEARCH_EXPONENTIAL_STRING_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

scl_error_t scl_search_exponential_string(const char **restrict strs, size_t count, const char *restrict key, size_t *restrict idx) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
