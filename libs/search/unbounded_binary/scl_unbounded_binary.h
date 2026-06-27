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

/* Unbounded binary search. O(log N). Exponential probe then binary on unknown-length array. */

#ifndef SCL_SEARCH_UNBOUNDED_BINARY_SEARCH_H
#define SCL_SEARCH_UNBOUNDED_BINARY_SEARCH_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

scl_error_t scl_search_unbounded_binary_search(scl_cmp_func_t cmp, const void * key, size_t * out_index, void *(*getter)(size_t index, void *ctx), void *ctx, size_t max_count) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
