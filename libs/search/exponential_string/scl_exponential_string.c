#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_exponential_string.h"
#include <string.h>

static scl_error_t binary_string_range(const char **strs, size_t lo, size_t hi, const char *key, size_t *idx)
{
    while (lo <= hi) {
        size_t mid = lo + (hi - lo) / 2;
        int r = strcmp(strs[mid], key);
        if (r == 0) { *idx = mid; return SCL_OK; }
        if (r < 0) lo = mid + 1;
        else hi = (mid == 0) ? 0 : mid - 1;
    }
    return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_search_exponential_string(const char **SCL_RESTRICT strs, size_t count, const char * key, size_t * idx)
{
    if (scl_unlikely(strs == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(key == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(idx == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(count == 0)) return SCL_ERR_EMPTY;

    if (strcmp(strs[0], key) == 0) { *idx = 0; return SCL_OK; }
    if (count == 1) return SCL_ERR_NOT_FOUND;

    size_t bound = 1;
    while (bound < count) {
        int r = strcmp(strs[bound], key);
        if (r >= 0)
            return binary_string_range(strs, bound / 2, bound, key, idx);
        bound *= 2;
    }
    return binary_string_range(strs, bound / 2, count - 1, key, idx);
}
