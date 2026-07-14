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

/* Bucket sort. O(N+k) avg. Distributes into buckets, sorts individually.
 * Stable.
 *
 * Security note: keys outside [0,1) are clamped to prevent out-of-bounds
 * bucket access from malicious input. */

#ifndef SCL_SORT_BUCKET_SORT_H
#define SCL_SORT_BUCKET_SORT_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

/*
 * Bucket sort requires a key-extractor function that maps each element
 * to a double-precision floating-point value in [0, 1).  The keys are
 * used to distribute elements into buckets in O(n) time — the comparator
 * is only used for within-bucket sorting.
 *
 * key_func: extracts a double key in [0, 1) from element at ptr.
 * cmp:      comparator for within-bucket sorting.
 */
typedef double (*scl_bucket_key_func_t)(const void *ptr);

scl_error_t scl_sort_bucket_sort(scl_allocator_t *alloc, void *base,
                                 size_t count, size_t element_size,
                                 scl_bucket_key_func_t key,
                                 scl_cmp_func_t cmp) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
