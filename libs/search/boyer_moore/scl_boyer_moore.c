#include "scl_boyer_moore.h"
#include <string.h>
#include <limits.h>

scl_error_t scl_search_boyer_moore(scl_allocator_t * alloc, const char * text, size_t tlen, const char * pat, size_t plen, size_t * pos)
{
    if (scl_unlikely(text == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(pat == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(pos == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(tlen == 0)) return SCL_ERR_EMPTY;
    if (scl_unlikely(plen == 0)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(plen > tlen)) return SCL_ERR_NOT_FOUND;

    int bad_char[UCHAR_MAX + 1];
    for (size_t i = 0; i <= UCHAR_MAX; i++) bad_char[i] = (int)plen;
    for (size_t i = 0; i < plen - 1; i++)
        bad_char[(unsigned char)pat[i]] = (int)(plen - 1 - i);

    int *suffix = (int *)scl_alloc(alloc, (plen + 1) * sizeof(int), alignof(max_align_t));
    int *gs = (int *)scl_alloc(alloc, (plen + 1) * sizeof(int), alignof(max_align_t));
    if (!suffix || !gs) { scl_free(alloc, suffix); scl_free(alloc, gs); return SCL_ERR_OUT_OF_MEMORY; }

    suffix[plen - 1] = (int)plen;
    int g = (int)(plen - 1);
    int f = (int)(plen - 1);
    for (int i = (int)plen - 2; i >= 0; i--) {
        if (i > g && suffix[(int)plen - 1 - f + i] < i - g)
            suffix[i] = suffix[(int)plen - 1 - f + i];
        else {
            if (i < g) g = i;
            f = i;
            while (g >= 0 && pat[g] == pat[(int)plen - 1 - f + g]) g--;
            suffix[i] = f - g;
        }
    }

    for (size_t i = 0; i <= plen; i++) gs[i] = (int)plen;
    for (int i = (int)plen - 1; i >= 0; i--)
        if (suffix[i] == i + 1)
            for (int j = 0; j < (int)plen - 1 - i; j++)
                if (gs[j] == (int)plen) gs[j] = (int)plen - 1 - i;
    for (size_t i = 0; i <= plen - 2; i++)
        gs[(int)plen - 1 - suffix[i]] = (int)plen - 1 - (int)i;

    size_t i = 0;
    scl_error_t result = SCL_ERR_NOT_FOUND;
    while (i <= tlen - plen) {
        int j = (int)plen - 1;
        while (j >= 0 && pat[j] == text[i + j]) j--;
        if (j < 0) {
            *pos = i;
            result = SCL_OK;
            break;
        }
        int shift_bc = bad_char[(unsigned char)text[i + j]] - ((int)plen - 1 - j);
        if (shift_bc < 1) shift_bc = 1;
        int shift_gs = gs[j];
        i += (shift_bc > shift_gs) ? (size_t)shift_bc : (size_t)shift_gs;
    }

    scl_free(alloc, suffix);
    scl_free(alloc, gs);
    return result;
}
