#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_interpolation.h"

scl_error_t scl_search_interpolation_search(const int64_t *restrict arr, size_t count, int64_t key, size_t *restrict out_index)
{
    if (__builtin_expect(arr == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(out_index == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(count == 0, 0)) return SCL_ERR_EMPTY;

    size_t lo = 0, hi = count - 1;

    while (lo <= hi && key >= arr[lo] && key <= arr[hi]) {
        if (__builtin_expect(lo == hi, 0)) {
            if (arr[lo] == key) {
                *out_index = lo;
                return SCL_OK;
            }
            break;
        }
        if (arr[hi] == arr[lo]) break;
        size_t pos = lo + (size_t)((double)(key - arr[lo]) * (double)(hi - lo) / (double)(arr[hi] - arr[lo]));
        if (pos < lo) pos = lo;
        if (pos > hi) pos = hi;

        if (arr[pos] == key) {
            *out_index = pos;
            return SCL_OK;
        }
        if (arr[pos] < key) lo = pos + 1;
        else hi = (pos == 0) ? 0 : pos - 1;
    }
    return SCL_ERR_NOT_FOUND;
}
