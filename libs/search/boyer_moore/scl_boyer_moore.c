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

/* Boyer-Moore pattern matcher. Sub-linear avg. Bad-character + good-suffix
 * shift tables. */

#include "scl_boyer_moore.h"
#include "scl_limits.h"
#include "scl_string.h"

/*
 * Shared core: scans `text` for `pat` using the bad-character and good-suffix
 * heuristics, reporting each match (including overlapping ones) through `cb`.
 * Building the two shift tables once and looping here lets both the
 * first-match and find-all entry points share all the work and the hardening.
 *
 * Security notes:
 *   - The good-suffix construction indexes with `int`, so a pattern longer than
 *     INT_MAX would overflow those indices — reject it up front.
 *   - Table sizing goes through scl_mul_overflow.
 *   - The bad-character table is indexed by `unsigned char`, immune to
 *     negative-char UB on platforms where `char` is signed.
 *
 * Always scans the whole text counting matches; a non-NULL `cb` is invoked per
 * match and may return false to stop early (the first-match wrapper uses that).
 */
static scl_error_t bm_core(scl_allocator_t *alloc, const char *text,
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
  if (scl_unlikely(plen > (size_t)INT_MAX))
    return SCL_ERR_SIZE_OVERFLOW;
  if (scl_unlikely(plen > tlen))
    return SCL_ERR_NOT_FOUND;

  int bad_char[UCHAR_MAX + 1];
  for (size_t i = 0; i <= UCHAR_MAX; i++)
    bad_char[i] = (int)plen;
  for (size_t i = 0; i < plen - 1; i++)
    bad_char[(unsigned char)pat[i]] = (int)(plen - 1 - i);

  size_t tbytes;
  if (scl_unlikely(scl_mul_overflow(plen + 1, sizeof(int), &tbytes)))
    return SCL_ERR_SIZE_OVERFLOW;
  int *suffix = (int *)scl_alloc(alloc, tbytes, alignof(max_align_t));
  int *gs = (int *)scl_alloc(alloc, tbytes, alignof(max_align_t));
  if (!suffix || !gs) {
    scl_free(alloc, suffix);
    scl_free(alloc, gs);
    return SCL_ERR_OUT_OF_MEMORY;
  }

  suffix[plen - 1] = (int)plen;
  int g = (int)(plen - 1);
  int f = (int)(plen - 1);
  for (int i = (int)plen - 2; i >= 0; i--) {
    if (i > g && suffix[(int)plen - 1 - f + i] < i - g)
      suffix[i] = suffix[(int)plen - 1 - f + i];
    else {
      if (i < g)
        g = i;
      f = i;
      while (g >= 0 && pat[g] == pat[(int)plen - 1 - f + g])
        g--;
      suffix[i] = f - g;
    }
  }

  for (size_t i = 0; i <= plen; i++)
    gs[i] = (int)plen;
  for (int i = (int)plen - 1; i >= 0; i--)
    if (suffix[i] == i + 1)
      for (int j = 0; j < (int)plen - 1 - i; j++)
        if (gs[j] == (int)plen)
          gs[j] = (int)plen - 1 - i;
  for (size_t i = 0; i <= plen - 2; i++)
    gs[(int)plen - 1 - suffix[i]] = (int)plen - 1 - (int)i;

  /* Shift applied after a complete match, to enumerate overlapping hits. */
  int match_shift = gs[0];
  if (match_shift < 1)
    match_shift = 1;

  size_t count = 0;
  size_t i = 0;
  scl_error_t result = SCL_ERR_NOT_FOUND;
  while (i <= tlen - plen) {
    int j = (int)plen - 1;
    while (j >= 0 && pat[j] == text[i + j])
      j--;
    if (j < 0) {
      count++;
      result = SCL_OK;
      if (cb && !cb(i, user))
        break; /* caller asked to stop */
      i += (size_t)match_shift;
    } else {
      int shift_bc = bad_char[(unsigned char)text[i + j]] - ((int)plen - 1 - j);
      if (shift_bc < 1)
        shift_bc = 1;
      int shift_gs = gs[j];
      i += (shift_bc > shift_gs) ? (size_t)shift_bc : (size_t)shift_gs;
    }
  }

  scl_free(alloc, suffix);
  scl_free(alloc, gs);
  if (out_count)
    *out_count = count;
  return result;
}

/* First-match adapter: capture the offset of the first hit. */
static bool bm_first_cb(size_t pos, void *user) {
  *(size_t *)user = pos;
  return false;
}

scl_error_t scl_search_boyer_moore(scl_allocator_t *alloc, const char *text,
                                   size_t tlen, const char *pat, size_t plen,
                                   size_t *pos) {
  if (scl_unlikely(pos == NULL))
    return SCL_ERR_NULL_PTR;
  size_t found = 0;
  scl_error_t r =
      bm_core(alloc, text, tlen, pat, plen, bm_first_cb, &found, NULL);
  if (r == SCL_OK)
    *pos = found;
  return r;
}

scl_error_t scl_search_boyer_moore_all(scl_allocator_t *alloc, const char *text,
                                       size_t tlen, const char *pat,
                                       size_t plen, scl_search_match_cb cb,
                                       void *user, size_t *out_count) {
  return bm_core(alloc, text, tlen, pat, plen, cb, user, out_count);
}
