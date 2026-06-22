#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_search_unbounded_binary_search.h"

scl_error_t scl_search_unbounded_binary_search(scl_cmp_func_t cmp, const void *restrict key, size_t *restrict out_index, void *(*getter)(size_t index, void *ctx), void *ctx, size_t max_count)
{
    if (__builtin_expect(cmp == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(key == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(out_index == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(getter == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(max_count == 0, 0)) return SCL_ERR_EMPTY;

    size_t lo = 0, hi = 1;
    void *elem = getter(0, ctx);
    if (__builtin_expect(elem == NULL, 0)) return SCL_ERR_NOT_FOUND;
    if (cmp(elem, key) == 0) {
        *out_index = 0;
        return SCL_OK;
    }

    while (hi < max_count) {
        elem = getter(hi, ctx);
        if (!elem) break;
        int r = cmp(elem, key);
        if (r == 0) {
            *out_index = hi;
            return SCL_OK;
        }
        if (r > 0) break;
        lo = hi;
        hi *= 2;
    }
    if (hi >= max_count) hi = max_count - 1;
    if (hi < lo) hi = lo;

    while (lo <= hi) {
        size_t mid = lo + (hi - lo) / 2;
        elem = getter(mid, ctx);
        if (!elem) { hi = (mid == 0) ? 0 : mid - 1; continue; }
        int r = cmp(elem, key);
        if (r == 0) {
            *out_index = mid;
            return SCL_OK;
        }
        if (r < 0) lo = mid + 1;
        else hi = (mid == 0) ? 0 : mid - 1;
    }
    return SCL_ERR_NOT_FOUND;
}
