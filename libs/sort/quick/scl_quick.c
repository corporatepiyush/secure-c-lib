#include "scl_quick.h"
#include <string.h>

static void swap(unsigned char *a, unsigned char *b, size_t element_size)
{
    while (element_size--) {
        unsigned char t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

scl_error_t scl_sort_quick_sort(void *base, size_t count, size_t element_size, scl_cmp_func_t cmp)
{
    if (!base || !cmp) return SCL_ERR_NULL_PTR;
    if (count < 2) return SCL_OK;

    long stack[256];
    int sp = -1;
    stack[++sp] = 0;
    stack[++sp] = (long)count - 1;

    while (sp >= 0) {
        long r = stack[sp--];
        long l = stack[sp--];

        if (l >= r) continue;

        unsigned char *pivot = (unsigned char *)base + r * element_size;
        long i = l - 1;

        for (long j = l; j < r; j++) {
            if (cmp((unsigned char *)base + j * element_size, pivot) < 0) {
                i++;
                if (i != j)
                    swap((unsigned char *)base + i * element_size,
                         (unsigned char *)base + j * element_size,
                         element_size);
            }
        }
        i++;
        if (i != r)
            swap((unsigned char *)base + i * element_size,
                 (unsigned char *)base + r * element_size,
                 element_size);

        if (i - 1 - l < r - (i + 1)) {
            stack[++sp] = i + 1; stack[++sp] = r;
            stack[++sp] = l; stack[++sp] = i - 1;
        } else {
            stack[++sp] = l; stack[++sp] = i - 1;
            stack[++sp] = i + 1; stack[++sp] = r;
        }
    }
    return SCL_OK;
}
