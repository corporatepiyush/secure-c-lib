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

#include "scl_kmp.h"
#include "scl_string.h"

/*
 * Shared core. Builds the prefix (lps) table once, then scans the text. On a
 * full match it reports the offset and resumes at lps[plen-1], which correctly
 * enumerates overlapping matches in O(N+M) total with no input that can force
 * quadratic behaviour (the classic KMP guarantee — important against
 * adversarial inputs that defeat naive matching).
 *
 * All indices are size_t, so there is no pattern-length cap or signed-index UB.
 * Allocation size is guarded with scl_mul_overflow.
 *
 * A non-NULL `cb` is invoked per match and may return false to stop early; the
 * first-match wrapper uses that.
 */
static scl_error_t kmp_core(scl_allocator_t *alloc, const char *text,
                            size_t tlen, const char *pat, size_t plen,
                            scl_search_match_cb cb, void *user,
                            size_t *out_count) {
  if (scl_unlikely(text == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(pat == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(tlen == 0))
    return SCL_ERR_EMPTY;
  if (scl_unlikely(plen == 0))
    return SCL_ERR_INVALID_ARG;
  if (scl_unlikely(plen > tlen))
    return SCL_ERR_NOT_FOUND;

  size_t lbytes;
  if (scl_unlikely(scl_mul_overflow(plen, sizeof(size_t), &lbytes)))
    return SCL_ERR_SIZE_OVERFLOW;
  size_t *lps =
      (size_t *)scl_calloc(alloc, plen, sizeof(size_t), alignof(max_align_t));
  if (scl_unlikely(lps == NULL))
    return SCL_ERR_OUT_OF_MEMORY;

  size_t len = 0, i = 1;
  while (i < plen) {
    if (pat[i] == pat[len]) {
      lps[i++] = ++len;
    } else if (len) {
      len = lps[len - 1];
    } else {
      lps[i++] = 0;
    }
  }

  size_t count = 0;
  i = 0;
  size_t j = 0;
  scl_error_t result = SCL_ERR_NOT_FOUND;
  while (i < tlen) {
    if (pat[j] == text[i]) {
      i++;
      j++;
      if (j == plen) {
        count++;
        result = SCL_OK;
        size_t pos = i - j;
        if (cb && !cb(pos, user))
          break;
        j = lps[j - 1]; /* resume: enumerate overlaps */
      }
    } else if (j) {
      j = lps[j - 1];
    } else {
      i++;
    }
  }

  scl_free(alloc, lps);
  if (out_count)
    *out_count = count;
  return result;
}

static bool kmp_first_cb(size_t pos, void *user) {
  *(size_t *)user = pos;
  return false;
}

scl_error_t scl_search_kmp(scl_allocator_t *alloc, const char *text,
                           size_t tlen, const char *pat, size_t plen,
                           size_t *pos) {
  if (scl_unlikely(pos == NULL))
    return SCL_ERR_NULL_PTR;
  size_t found = 0;
  scl_error_t r =
      kmp_core(alloc, text, tlen, pat, plen, kmp_first_cb, &found, NULL);
  if (r == SCL_OK)
    *pos = found;
  return r;
}

scl_error_t scl_search_kmp_all(scl_allocator_t *alloc, const char *text,
                               size_t tlen, const char *pat, size_t plen,
                               scl_search_match_cb cb, void *user,
                               size_t *out_count) {
  return kmp_core(alloc, text, tlen, pat, plen, cb, user, out_count);
}
