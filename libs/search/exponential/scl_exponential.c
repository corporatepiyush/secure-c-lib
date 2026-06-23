#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_exponential.h"

static scl_error_t binary_search_range(const void *base, size_t lo, size_t hi, size_t elem_size, const void *key, scl_cmp_func_t cmp, size_t *out_index)
{
    while (lo <= hi) {
        size_t mid = lo + (hi - lo) / 2;
        const void *elem = (const unsigned char *)base + mid * elem_size;
        int r = cmp(elem, key);
        if (r == 0) { *out_index = mid; return SCL_OK; }
        if (r < 0) lo = mid + 1;
        else hi = (mid == 0) ? 0 : mid - 1;
    }
    return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_search_exponential_search(const void *restrict base, size_t count, size_t elem_size, const void *restrict key, scl_cmp_func_t cmp, size_t *restrict out_index)
{
    if (__builtin_expect(base == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(key == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(cmp == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(out_index == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(count == 0, 0)) return SCL_ERR_EMPTY;

    if (cmp(base, key) == 0) {
        *out_index = 0;
        return SCL_OK;
    }
    if (count == 1) return SCL_ERR_NOT_FOUND;

    size_t bound = 1;
    const unsigned char *bytes = (const unsigned char *)base;
    while (bound < count) {
        const void *elem = bytes + bound * elem_size;
        int r = cmp(elem, key);
        if (r >= 0) {
            size_t hi = bound;
            if (hi >= count) hi = count - 1;
            return binary_search_range(base, bound / 2, hi, elem_size, key, cmp, out_index);
        }
        bound *= 2;
    }
    return binary_search_range(base, bound / 2, count - 1, elem_size, key, cmp, out_index);
}
