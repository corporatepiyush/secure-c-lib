/*
 * Copyright 2026 Piyush Katariya
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Quicksort. O(N log N) avg. Median-of-three pivot. In-place, iterative. */

#include "scl_quick.h"
#include "scl_string.h"

static SCL_ALWAYS_INLINE void swap(unsigned char *a, unsigned char *b,
                                   size_t element_size) {
  while (element_size--) {
    unsigned char t = *a;
    *a++ = *b;
    *b++ = t;
  }
}

/* Median-of-three pivot selection to avoid O(N^2) on sorted/reverse-sorted
 * or adversarial inputs. */
static unsigned char *median_of_three(unsigned char *base, size_t a, size_t b,
                                      size_t c, size_t element_size,
                                      scl_cmp_func_t cmp) {
  unsigned char *pa = base + a * element_size;
  unsigned char *pb = base + b * element_size;
  unsigned char *pc = base + c * element_size;
  if (cmp(pa, pb) < 0) {
    if (cmp(pb, pc) < 0)
      return pb;
    return (cmp(pa, pc) < 0) ? pc : pa;
  } else {
    if (cmp(pa, pc) < 0)
      return pa;
    return (cmp(pb, pc) < 0) ? pc : pb;
  }
}

scl_error_t scl_sort_quick_sort(void *base, size_t count, size_t element_size,
                                scl_cmp_func_t cmp) {
  if (scl_unlikely(!base || !cmp))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(count < 2))
    return SCL_OK;

  long stack[256];
  int sp = -1;
  stack[++sp] = 0;
  stack[++sp] = (long)count - 1;

  /* Use insertion sort for small partitions. */
  while (scl_likely(sp >= 0)) {
    long r = stack[sp--];
    long l = stack[sp--];

    if (scl_unlikely(l >= r))
      continue;

/* Insertion sort for small ranges (<= 16 elements). */
     if (r - l + 1 <= 16) {
       unsigned char *bp = (unsigned char *)base;
       unsigned char key[SCL_SORT_QUICK_STACK_KEY_SIZE];
       void *heap_key = NULL;
       if (element_size > sizeof(key)) {
         heap_key = scl_alloc(NULL, element_size, _Alignof(max_align_t));
         if (!heap_key)
           return SCL_ERR_OUT_OF_MEMORY;
       }
       void *kptr = heap_key ? heap_key : key;
       for (long i = l + 1; i <= r; i++) {
         scl_memcpy(kptr, bp + i * element_size, element_size);
         long j = i - 1;
         while (j >= l && cmp(bp + j * element_size, kptr) > 0) {
           scl_memcpy(bp + (j + 1) * element_size, bp + j * element_size,
                      element_size);
           j--;
         }
         scl_memcpy(bp + (j + 1) * element_size, kptr, element_size);
       }
       scl_free(NULL, heap_key);
       continue;
     }

    /* Median-of-three pivot selection. */
    long mid = l + (r - l) / 2;
    unsigned char *pivot = median_of_three(base, (size_t)l, (size_t)mid,
                                           (size_t)r, element_size, cmp);
    long p_idx =
        (long)(((unsigned char *)pivot - (unsigned char *)base) / element_size);

    /* Move pivot to end. */
    if (p_idx != r)
      swap((unsigned char *)base + p_idx * element_size,
           (unsigned char *)base + r * element_size, element_size);

    long i = l - 1;
    for (long j = l; j < r; j++) {
      if (cmp((unsigned char *)base + j * element_size, pivot) < 0) {
        i++;
        if (scl_unlikely(i != j))
          swap((unsigned char *)base + i * element_size,
               (unsigned char *)base + j * element_size, element_size);
      }
    }
    i++;
    if (scl_unlikely(i != r))
      swap((unsigned char *)base + i * element_size,
           (unsigned char *)base + r * element_size, element_size);

    /* Push smaller partition first to limit stack depth. */
    if (scl_likely(i - 1 - l < r - (i + 1))) {
      stack[++sp] = i + 1;
      stack[++sp] = r;
      stack[++sp] = l;
      stack[++sp] = i - 1;
    } else {
      stack[++sp] = l;
      stack[++sp] = i - 1;
      stack[++sp] = i + 1;
      stack[++sp] = r;
    }
  }
  return SCL_OK;
}
