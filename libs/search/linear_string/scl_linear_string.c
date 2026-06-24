#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_linear_string.h"
#include <string.h>

scl_error_t scl_search_linear_string(const char **SCL_RESTRICT strs, size_t count, const char * key, size_t * idx)
{
    if (scl_unlikely(strs == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(key == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(idx == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(count == 0)) return SCL_ERR_EMPTY;

    for (size_t i = 0; i < count; i++) {
        if (!strs[i]) continue;
        if (strcmp(strs[i], key) == 0) {
            *idx = i;
            return SCL_OK;
        }
    }
    return SCL_ERR_NOT_FOUND;
}
