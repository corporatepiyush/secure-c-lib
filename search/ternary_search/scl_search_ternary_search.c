#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_search_ternary_search.h"

scl_error_t scl_search_ternary_search(const void *restrict base, size_t count, size_t elem_size, const void *restrict key, scl_cmp_func_t cmp, size_t *restrict out_index)
{
    if (__builtin_expect(base == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(key == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(cmp == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(out_index == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(count == 0, 0)) return SCL_ERR_EMPTY;

    size_t lo = 0, hi = count - 1;
    const unsigned char *bytes = (const unsigned char *)base;

    while (lo <= hi) {
        size_t mid1 = lo + (hi - lo) / 3;
        size_t mid2 = hi - (hi - lo) / 3;

        const void *e1 = bytes + mid1 * elem_size;
        int r1 = cmp(e1, key);
        if (r1 == 0) { *out_index = mid1; return SCL_OK; }

        const void *e2 = bytes + mid2 * elem_size;
        int r2 = cmp(e2, key);
        if (r2 == 0) { *out_index = mid2; return SCL_OK; }

        if (r1 < 0 && r2 > 0) {
            lo = mid1 + 1;
            hi = (mid2 == 0) ? 0 : mid2 - 1;
        } else if (r2 < 0) {
            lo = mid2 + 1;
        } else {
            hi = (mid1 == 0) ? 0 : mid1 - 1;
        }
    }
    return SCL_ERR_NOT_FOUND;
}
