#ifndef SCL_SEARCH_LINEAR_STRING_H
#define SCL_SEARCH_LINEAR_STRING_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

scl_error_t scl_search_linear_string(const char **SCL_RESTRICT strs, size_t count, const char * key, size_t * idx) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
