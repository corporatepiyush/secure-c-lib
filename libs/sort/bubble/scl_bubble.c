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

/* Bubble sort. O(N^2). Stable, in-place. Early-exit optimisation on sorted
 * runs. */

#include "scl_bubble.h"
#include "scl_stdbool.h"
#include "scl_string.h"

static SCL_ALWAYS_INLINE void swap(unsigned char *a, unsigned char *b,
                                   size_t element_size) {
  while (element_size--) {
    unsigned char t = *a;
    *a++ = *b;
    *b++ = t;
  }
}

scl_error_t scl_sort_bubble_sort(void *base, size_t count, size_t element_size,
                                 scl_cmp_func_t cmp) {
  if (scl_unlikely(!base || !cmp))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(count < 2))
    return SCL_OK;

  bool swapped;
  for (size_t i = 0; i < count - 1; i++) {
    swapped = false;
    for (size_t j = 0; j < count - 1 - i; j++) {
      if (cmp((unsigned char *)base + (j + 1) * element_size,
              (unsigned char *)base + j * element_size) < 0) {
        swap((unsigned char *)base + j * element_size,
             (unsigned char *)base + (j + 1) * element_size, element_size);
        swapped = true;
      }
    }
    if (scl_unlikely(!swapped))
      break;
  }
  return SCL_OK;
}
