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

/* LSD radix sort. O(d(N+k)). Non-comparison. Stable digit-by-digit pass. */

#ifndef SCL_SORT_RADIX_SORT_H
#define SCL_SORT_RADIX_SORT_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

scl_error_t scl_sort_radix_sort(scl_allocator_t *alloc, int32_t * base, size_t count) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
