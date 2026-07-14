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

/* KMP pattern matcher. O(N+M). Prefix function (pi) skips backtracking on
 * mismatch. */

#ifndef SCL_SEARCH_KMP_H
#define SCL_SEARCH_KMP_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"
#include "scl_stdbool.h"

/* Callback invoked once per match with the 0-based start offset. Return false
 * to stop the search early, true to continue. Shared by the string matchers. */
#ifndef SCL_SEARCH_MATCH_CB_DEFINED
#define SCL_SEARCH_MATCH_CB_DEFINED
typedef bool (*scl_search_match_cb)(size_t pos, void *user);
#endif

/* Find the first occurrence of pat in text. *pos receives the offset.
 * Returns SCL_OK, SCL_ERR_NOT_FOUND, or an error. */
scl_error_t scl_search_kmp(scl_allocator_t *alloc, const char *text,
                           size_t tlen, const char *pat, size_t plen,
                           size_t *pos) SCL_WARN_UNUSED;

/* Report every occurrence of pat in text (including overlapping ones) via cb,
 * building the prefix table once. *out_count (optional) receives the number of
 * matches. cb may be NULL to only count. Returns SCL_OK if at least one match
 * was found, SCL_ERR_NOT_FOUND if none, or an error. */
scl_error_t scl_search_kmp_all(scl_allocator_t *alloc, const char *text,
                               size_t tlen, const char *pat, size_t plen,
                               scl_search_match_cb cb, void *user,
                               size_t *out_count) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
