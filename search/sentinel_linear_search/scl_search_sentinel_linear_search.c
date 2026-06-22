#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_search_sentinel_linear_search.h"
#include <string.h>
#include <stdlib.h>

scl_error_t scl_search_sentinel_linear_search(const void *restrict base, size_t count, size_t elem_size, const void *restrict key, scl_cmp_func_t cmp, size_t *restrict out_index)
{
    if (__builtin_expect(base == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(key == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(cmp == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(out_index == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(count == 0, 0)) return SCL_ERR_EMPTY;

    unsigned char *buf = (unsigned char *)malloc(elem_size);
    if (__builtin_expect(buf == NULL, 0)) return SCL_ERR_OUT_OF_MEMORY;
    memcpy(buf, key, elem_size);

    unsigned char *copy = (unsigned char *)malloc(count * elem_size);
    if (__builtin_expect(copy == NULL, 0)) { free(buf); return SCL_ERR_OUT_OF_MEMORY; }
    memcpy(copy, base, count * elem_size);

    size_t i = 0;
    while (1) {
        if (i < count) {
            int r = cmp(copy + i * elem_size, buf);
            if (r == 0) break;
        }
        i++;
        if (i > count) break;
    }

    free(buf);
    free(copy);

    if (i < count) {
        *out_index = i;
        return SCL_OK;
    }
    return SCL_ERR_NOT_FOUND;
}
