#include "scl_search_kmp.h"
#include <string.h>

scl_error_t scl_search_kmp(scl_allocator_t *alloc, const char *restrict text, size_t tlen, const char *restrict pat, size_t plen, size_t *restrict pos)
{
    if (__builtin_expect(text == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(pat == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(pos == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(tlen == 0, 0)) return SCL_ERR_EMPTY;
    if (__builtin_expect(plen == 0, 0)) return SCL_ERR_INVALID_ARG;
    if (__builtin_expect(plen > tlen, 0)) return SCL_ERR_NOT_FOUND;

    size_t *lps = (size_t *)scl_calloc(alloc, plen, sizeof(size_t), alignof(max_align_t));
    if (__builtin_expect(lps == NULL, 0)) return SCL_ERR_OUT_OF_MEMORY;

    size_t len = 0, i = 1;
    while (i < plen) {
        if (pat[i] == pat[len]) {
            lps[i++] = ++len;
        } else if (len) {
            len = lps[len - 1];
        } else {
            lps[i++] = 0;
        }
    }

    i = 0;
    size_t j = 0;
    scl_error_t result = SCL_ERR_NOT_FOUND;
    while (i < tlen) {
        if (pat[j] == text[i]) {
            i++; j++;
        }
        if (j == plen) {
            *pos = i - j;
            result = SCL_OK;
            break;
        } else if (i < tlen && pat[j] != text[i]) {
            if (j) j = lps[j - 1];
            else i++;
        }
    }

    scl_free(alloc, lps);
    return result;
}
