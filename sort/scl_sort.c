#include "scl_sort.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static void scl_sort_swap(unsigned char *a, unsigned char *b, size_t element_size)
{
    while (element_size--) {
        unsigned char t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

static void scl_sort_copy(unsigned char *dest, const unsigned char *src, size_t element_size)
{
    memcpy(dest, src, element_size);
}

scl_error_t scl_sort_quick(void *base, size_t count, size_t element_size, scl_cmp_func_t cmp)
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
                    scl_sort_swap((unsigned char *)base + i * element_size,
                                  (unsigned char *)base + j * element_size,
                                  element_size);
            }
        }
        i++;
        if (i != r)
            scl_sort_swap((unsigned char *)base + i * element_size,
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

scl_error_t scl_sort_merge(scl_allocator_t *alloc, void *base, size_t count, size_t element_size, scl_cmp_func_t cmp)
{
    if (!base || !cmp) return SCL_ERR_NULL_PTR;
    if (count < 2) return SCL_OK;

    size_t bytes;
    if (scl_mul_overflow(count, element_size, &bytes))
        return SCL_ERR_SIZE_OVERFLOW;

    unsigned char *tmp = (unsigned char *)scl_alloc(alloc, bytes, alignof(max_align_t));
    if (!tmp) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t width = 1; width < count; width *= 2) {
        for (size_t i = 0; i < count; i += 2 * width) {
            size_t mid = i + width;
            if (mid >= count) continue;
            size_t r = i + 2 * width;
            if (r > count) r = count;

            size_t li = i, ri = mid, ti = 0;

            while (li < mid && ri < r) {
                if (cmp((unsigned char *)base + li * element_size,
                        (unsigned char *)base + ri * element_size) <= 0)
                    scl_sort_copy(tmp + ti++ * element_size,
                                  (unsigned char *)base + li++ * element_size,
                                  element_size);
                else
                    scl_sort_copy(tmp + ti++ * element_size,
                                  (unsigned char *)base + ri++ * element_size,
                                  element_size);
            }
            while (li < mid)
                scl_sort_copy(tmp + ti++ * element_size,
                              (unsigned char *)base + li++ * element_size,
                              element_size);
            while (ri < r)
                scl_sort_copy(tmp + ti++ * element_size,
                              (unsigned char *)base + ri++ * element_size,
                              element_size);

            memcpy((unsigned char *)base + i * element_size, tmp, ti * element_size);
        }
    }

    scl_free(alloc, tmp);
    return SCL_OK;
}

static void scl_heap_sift_down(unsigned char *base, size_t start, size_t end,
                                size_t element_size, scl_cmp_func_t cmp)
{
    size_t root = start;
    while (root * 2 + 1 <= end) {
        size_t child = root * 2 + 1;
        size_t swp = root;
        if (cmp(base + swp * element_size, base + child * element_size) < 0)
            swp = child;
        if (child + 1 <= end &&
            cmp(base + swp * element_size, base + (child + 1) * element_size) < 0)
            swp = child + 1;
        if (swp == root) break;
        scl_sort_swap(base + root * element_size, base + swp * element_size, element_size);
        root = swp;
    }
}

scl_error_t scl_sort_heap(void *base, size_t count, size_t element_size, scl_cmp_func_t cmp)
{
    unsigned char *ptr = (unsigned char *)base;
    if (!base || !cmp) return SCL_ERR_NULL_PTR;
    if (count < 2) return SCL_OK;

    for (size_t i = count / 2; i > 0; i--)
        scl_heap_sift_down(ptr, i - 1, count - 1, element_size, cmp);

    for (size_t i = count - 1; i > 0; i--) {
        scl_sort_swap(ptr, ptr + i * element_size, element_size);
        scl_heap_sift_down(ptr, 0, i - 1, element_size, cmp);
    }

    return SCL_OK;
}

scl_error_t scl_sort_insertion(void *base, size_t count, size_t element_size, scl_cmp_func_t cmp)
{
    if (!base || !cmp) return SCL_ERR_NULL_PTR;
    if (count < 2) return SCL_OK;

    for (size_t i = 1; i < count; i++) {
        size_t j = i;
        while (j > 0 && cmp((unsigned char *)base + j * element_size,
                            (unsigned char *)base + (j - 1) * element_size) < 0) {
            scl_sort_swap((unsigned char *)base + j * element_size,
                          (unsigned char *)base + (j - 1) * element_size,
                          element_size);
            j--;
        }
    }
    return SCL_OK;
}

scl_error_t scl_sort_selection(void *base, size_t count, size_t element_size, scl_cmp_func_t cmp)
{
    if (!base || !cmp) return SCL_ERR_NULL_PTR;
    if (count < 2) return SCL_OK;

    for (size_t i = 0; i < count - 1; i++) {
        size_t min_idx = i;
        for (size_t j = i + 1; j < count; j++) {
            if (cmp((unsigned char *)base + j * element_size,
                    (unsigned char *)base + min_idx * element_size) < 0)
                min_idx = j;
        }
        if (min_idx != i)
            scl_sort_swap((unsigned char *)base + i * element_size,
                          (unsigned char *)base + min_idx * element_size,
                          element_size);
    }
    return SCL_OK;
}

scl_error_t scl_sort_bubble(void *base, size_t count, size_t element_size, scl_cmp_func_t cmp)
{
    if (!base || !cmp) return SCL_ERR_NULL_PTR;
    if (count < 2) return SCL_OK;

    bool swapped;
    for (size_t i = 0; i < count - 1; i++) {
        swapped = false;
        for (size_t j = 0; j < count - 1 - i; j++) {
            if (cmp((unsigned char *)base + (j + 1) * element_size,
                    (unsigned char *)base + j * element_size) < 0) {
                scl_sort_swap((unsigned char *)base + j * element_size,
                              (unsigned char *)base + (j + 1) * element_size,
                              element_size);
                swapped = true;
            }
        }
        if (!swapped) break;
    }
    return SCL_OK;
}

scl_error_t scl_sort_shell(void *base, size_t count, size_t element_size, scl_cmp_func_t cmp)
{
    if (!base || !cmp) return SCL_ERR_NULL_PTR;
    if (count < 2) return SCL_OK;

    for (size_t gap = count / 2; gap > 0; gap /= 2) {
        for (size_t i = gap; i < count; i++) {
            size_t j = i;
            while (j >= gap &&
                   cmp((unsigned char *)base + j * element_size,
                       (unsigned char *)base + (j - gap) * element_size) < 0) {
                scl_sort_swap((unsigned char *)base + j * element_size,
                              (unsigned char *)base + (j - gap) * element_size,
                              element_size);
                j -= gap;
            }
        }
    }
    return SCL_OK;
}

scl_error_t scl_sort_counting(scl_allocator_t *alloc, int32_t *base, size_t count)
{
    if (!base) return SCL_ERR_NULL_PTR;
    if (count < 2) return SCL_OK;

    int32_t min_val = base[0], max_val = base[0];
    for (size_t i = 1; i < count; i++) {
        if (base[i] < min_val) min_val = base[i];
        if (base[i] > max_val) max_val = base[i];
    }

    size_t range;
    if (scl_add_overflow((size_t)(max_val - min_val), 1, &range))
        return SCL_ERR_SIZE_OVERFLOW;

    size_t *counts = (size_t *)scl_calloc(alloc, range, sizeof(size_t), alignof(max_align_t));
    if (!counts) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t i = 0; i < count; i++)
        counts[(size_t)(base[i] - min_val)]++;

    size_t idx = 0;
    for (size_t i = 0; i < range; i++) {
        for (size_t j = 0; j < counts[i]; j++)
            base[idx++] = (int32_t)((size_t)min_val + i);
    }

    scl_free(alloc, counts);
    return SCL_OK;
}

scl_error_t scl_sort_radix(scl_allocator_t *alloc, int32_t *base, size_t count)
{
    if (!base) return SCL_ERR_NULL_PTR;
    if (count < 2) return SCL_OK;

    size_t bytes;
    if (scl_mul_overflow(count, sizeof(int32_t), &bytes))
        return SCL_ERR_SIZE_OVERFLOW;
    int32_t *output = (int32_t *)scl_alloc(alloc, bytes, alignof(max_align_t));
    if (!output) return SCL_ERR_OUT_OF_MEMORY;

    for (int shift = 0; shift < 32; shift += 8) {
        size_t bucket[256] = {0};

        for (size_t i = 0; i < count; i++) {
            int32_t val = base[i] >> shift;
            bucket[(unsigned char)val]++;
        }

        size_t total = 0;
        for (int i = 0; i < 256; i++) {
            size_t old = bucket[i];
            bucket[i] = total;
            total += old;
        }

        for (size_t i = 0; i < count; i++) {
            int32_t val = base[i] >> shift;
            output[bucket[(unsigned char)val]++] = base[i];
        }

        memcpy(base, output, bytes);
    }

    scl_free(alloc, output);
    return SCL_OK;
}

scl_error_t scl_sort_bucket(scl_allocator_t *alloc, void *base, size_t count, size_t element_size, scl_cmp_func_t cmp)
{
    unsigned char *ptr = (unsigned char *)base;
    if (!base || !cmp) return SCL_ERR_NULL_PTR;
    if (count < 2) return SCL_OK;

    size_t bucket_count = count < 10 ? count : 10;
    unsigned char **buckets = (unsigned char **)scl_calloc(alloc, bucket_count, sizeof(unsigned char *), alignof(max_align_t));
    size_t *bucket_sizes = (size_t *)scl_calloc(alloc, bucket_count, sizeof(size_t), alignof(max_align_t));
    size_t *bucket_caps = (size_t *)scl_calloc(alloc, bucket_count, sizeof(size_t), alignof(max_align_t));

    if (!buckets || !bucket_sizes || !bucket_caps) {
        scl_free(alloc, buckets); scl_free(alloc, bucket_sizes); scl_free(alloc, bucket_caps);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    size_t *less_count = (size_t *)scl_calloc(alloc, count, sizeof(size_t), alignof(max_align_t));
    if (!less_count) {
        scl_free(alloc, buckets); scl_free(alloc, bucket_sizes); scl_free(alloc, bucket_caps);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < count; j++) {
            if (cmp(ptr + i * element_size, ptr + j * element_size) > 0)
                less_count[i]++;
        }
    }

    for (size_t i = 0; i < count; i++) {
        size_t bucket_idx = (less_count[i] * bucket_count) / count;
        if (bucket_idx >= bucket_count) bucket_idx = bucket_count - 1;

        if (bucket_sizes[bucket_idx] == bucket_caps[bucket_idx]) {
            size_t new_cap = bucket_caps[bucket_idx] == 0 ? 4 : bucket_caps[bucket_idx] * 2;
            size_t new_bytes;
            if (scl_mul_overflow(new_cap, element_size, &new_bytes)) {
                for (size_t j = 0; j < bucket_count; j++) scl_free(alloc, buckets[j]);
                scl_free(alloc, buckets); scl_free(alloc, bucket_sizes); scl_free(alloc, bucket_caps); scl_free(alloc, less_count);
                return SCL_ERR_SIZE_OVERFLOW;
            }
            unsigned char *tmp = (unsigned char *)scl_alloc(alloc, new_bytes, alignof(max_align_t));
            if (!tmp) {
                for (size_t j = 0; j < bucket_count; j++) scl_free(alloc, buckets[j]);
                scl_free(alloc, buckets); scl_free(alloc, bucket_sizes); scl_free(alloc, bucket_caps); scl_free(alloc, less_count);
                return SCL_ERR_OUT_OF_MEMORY;
            }
            if (buckets[bucket_idx] && bucket_sizes[bucket_idx] > 0)
                memcpy(tmp, buckets[bucket_idx], bucket_sizes[bucket_idx] * element_size);
            scl_free(alloc, buckets[bucket_idx]);
            buckets[bucket_idx] = tmp;
            bucket_caps[bucket_idx] = new_cap;
        }

        memcpy(buckets[bucket_idx] + bucket_sizes[bucket_idx] * element_size,
               ptr + i * element_size, element_size);
        bucket_sizes[bucket_idx]++;
    }

    scl_free(alloc, less_count);

    size_t pos = 0;
    for (size_t i = 0; i < bucket_count; i++) {
        if (bucket_sizes[i] > 0) {
            (void)scl_sort_insertion(buckets[i], bucket_sizes[i], element_size, cmp);
            memcpy(ptr + pos * element_size, buckets[i],
                   bucket_sizes[i] * element_size);
            pos += bucket_sizes[i];
        }
        scl_free(alloc, buckets[i]);
    }

    scl_free(alloc, buckets);
    scl_free(alloc, bucket_sizes);
    scl_free(alloc, bucket_caps);
    return SCL_OK;
}
