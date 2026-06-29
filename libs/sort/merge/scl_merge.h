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

/* Mergesort. O(N log N). Stable. Top-down or bottom-up. O(N) auxiliary space.
 * Also provides a multithreaded variant (parallel divide-and-conquer). */

#ifndef SCL_SORT_MERGE_SORT_H
#define SCL_SORT_MERGE_SORT_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

scl_error_t scl_sort_merge_sort(scl_allocator_t *alloc, void * base, size_t count, size_t element_size, scl_cmp_func_t cmp) SCL_WARN_UNUSED;

/*
 * Multithreaded (parallel) mergesort. Same contract and stable ordering as
 * scl_sort_merge_sort, but the divide-and-conquer recursion runs across worker
 * threads.
 *
 *   max_threads  upper bound on threads to use. 0 => auto-detect the online
 *                CPU count. Internally clamped to [1, SCL_SORT_MERGE_MAX_THREADS];
 *                a value of 1 (or a small array) runs fully sequentially.
 *
 * Security / robustness:
 *   - Thread count is bounded (no unbounded thread creation regardless of input).
 *   - A single O(N) temp buffer is shared and partitioned by disjoint index
 *     range per subtask, so there are no data races (clean under TSan).
 *   - Overflow-safe sizing (scl_mul_overflow on count*element_size).
 *   - Falls back to sequential sorting for any subrange whose thread cannot be
 *     created, so the sort always completes correctly under thread exhaustion.
 */
#define SCL_SORT_MERGE_MAX_THREADS 128u
scl_error_t scl_sort_merge_sort_mt(scl_allocator_t *alloc, void * base, size_t count, size_t element_size, scl_cmp_func_t cmp, unsigned int max_threads) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
