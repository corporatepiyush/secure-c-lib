#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_search_rabin_karp.h"
#include <stdlib.h>

#define RK_BASE 256
#define RK_MOD 1000000007

scl_error_t scl_search_rabin_karp(const char *restrict text, size_t tlen, const char *restrict pat, size_t plen, size_t *restrict pos)
{
    if (__builtin_expect(text == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(pat == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(pos == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(tlen == 0, 0)) return SCL_ERR_EMPTY;
    if (__builtin_expect(plen == 0, 0)) return SCL_ERR_INVALID_ARG;
    if (__builtin_expect(plen > tlen, 0)) return SCL_ERR_NOT_FOUND;

    int64_t pat_hash = 0, text_hash = 0, h = 1;

    for (size_t i = 0; i < plen - 1; i++)
        h = (h * RK_BASE) % RK_MOD;

    for (size_t i = 0; i < plen; i++) {
        pat_hash = (pat_hash * RK_BASE + pat[i]) % RK_MOD;
        text_hash = (text_hash * RK_BASE + text[i]) % RK_MOD;
    }

    for (size_t i = 0; i <= tlen - plen; i++) {
        if (pat_hash == text_hash) {
            size_t j;
            for (j = 0; j < plen; j++)
                if (text[i + j] != pat[j]) break;
            if (j == plen) {
                *pos = i;
                return SCL_OK;
            }
        }
        if (i < tlen - plen) {
            text_hash = (RK_BASE * (text_hash - text[i] * h) + text[i + plen]) % RK_MOD;
            if (text_hash < 0) text_hash += RK_MOD;
        }
    }
    return SCL_ERR_NOT_FOUND;
}
