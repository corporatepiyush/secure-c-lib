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

/* Quickselect. O(N) avg. Hoare/Lomuto partition. kth smallest in-place. */

#include "scl_quickselect.h"
#include "scl_string.h"

static void swap_elements(void *a, void *b, size_t elem_size) {
  if (a == b)
    return;
  size_t i = 0;
  while (i < elem_size) {
    unsigned char t = ((unsigned char *)a)[i];
    ((unsigned char *)a)[i] = ((unsigned char *)b)[i];
    ((unsigned char *)b)[i] = t;
    i++;
  }
}

/*
 * Choose the median of base[lo], base[mid], base[hi] and move it to `hi` to use
 * as the pivot. A fixed last-element pivot makes quickselect degrade to O(n^2)
 * on sorted, reverse-sorted, or organ-pipe inputs — an algorithmic-complexity
 * DoS for any caller that selects over attacker-influenced data.
 * Median-of-three keeps those common adversarial shapes near the O(n) expected
 * case.
 */
static void median_of_three(unsigned char *bytes, size_t lo, size_t hi,
                            size_t elem_size, scl_cmp_func_t cmp) {
  size_t mid = lo + (hi - lo) / 2;
  void *a = bytes + lo * elem_size;
  void *b = bytes + mid * elem_size;
  void *c = bytes + hi * elem_size;
  /* Sort the three so the median ends up at `mid`, then swap median to hi. */
  if (cmp(a, b) > 0)
    swap_elements(a, b, elem_size);
  if (cmp(b, c) > 0)
    swap_elements(b, c, elem_size);
  if (cmp(a, b) > 0)
    swap_elements(a, b, elem_size);
  swap_elements(b, c, elem_size); /* median (now at mid) -> hi */
}

static size_t partition(void *base, size_t lo, size_t hi, size_t elem_size,
                        scl_cmp_func_t cmp) {
  unsigned char *bytes = (unsigned char *)base;
  if (hi - lo >= 2)
    median_of_three(bytes, lo, hi, elem_size, cmp);
  void *pivot = bytes + hi * elem_size;
  size_t i = lo;
  for (size_t j = lo; j < hi; j++) {
    if (cmp(bytes + j * elem_size, pivot) < 0) {
      swap_elements(bytes + i * elem_size, bytes + j * elem_size, elem_size);
      i++;
    }
  }
  swap_elements(bytes + i * elem_size, bytes + hi * elem_size, elem_size);
  return i;
}

scl_error_t scl_search_quickselect(scl_allocator_t *alloc, void *base,
                                   size_t count, size_t elem_size,
                                   scl_cmp_func_t cmp, size_t k, void *out) {
  if (scl_unlikely(base == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(cmp == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(out == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(count == 0))
    return SCL_ERR_EMPTY;
  if (scl_unlikely(k >= count))
    return SCL_ERR_INVALID_INDEX;

  size_t bytes;
  if (scl_mul_overflow(count, elem_size, &bytes))
    return SCL_ERR_SIZE_OVERFLOW;
  void *copy = scl_alloc(alloc, bytes, alignof(max_align_t));
  if (scl_unlikely(copy == NULL))
    return SCL_ERR_OUT_OF_MEMORY;
  scl_memcpy(copy, base, bytes);

  size_t lo = 0, hi = count - 1;
  while (lo < hi) {
    size_t p = partition(copy, lo, hi, elem_size, cmp);
    if (p == k)
      break;
    if (p > k)
      hi = (p == 0) ? 0 : p - 1;
    else
      lo = p + 1;
  }

  scl_memcpy(out, (unsigned char *)copy + k * elem_size, elem_size);
  scl_free(alloc, copy);
  return SCL_OK;
}
