#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_meta_binary.h"

scl_error_t scl_search_meta_binary_search(const int32_t * arr, size_t count, int32_t key, size_t * out_index)
{
    if (scl_unlikely(arr == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(out_index == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(count == 0)) return SCL_ERR_EMPTY;

    size_t bit_count = 0;
    size_t tmp = count;
    while (tmp) { bit_count++; tmp >>= 1; }

    size_t pos = 0;
    for (size_t b = bit_count; b > 0; b--) {
        size_t step = 1ULL << (b - 1);
        size_t mid = pos + step;
        if (mid >= count) continue;
        if (arr[mid] <= key)
            pos = mid;
    }

    if (arr[pos] == key) {
        *out_index = pos;
        return SCL_OK;
    }
    return SCL_ERR_NOT_FOUND;
}
