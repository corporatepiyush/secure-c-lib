#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_search_binary_search.h"

scl_error_t scl_search_binary_search(const void *restrict base, size_t count, size_t elem_size, const void *restrict key, scl_cmp_func_t cmp, size_t *restrict out_index)
{
    if (__builtin_expect(base == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(key == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(cmp == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(out_index == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(count == 0, 0)) return SCL_ERR_EMPTY;

    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const void *elem = (const unsigned char *)base + mid * elem_size;
        int r = cmp(elem, key);
        if (r == 0) {
            *out_index = mid;
            return SCL_OK;
        }
        if (r < 0) lo = mid + 1;
        else hi = mid;
    }
    return SCL_ERR_NOT_FOUND;
}
