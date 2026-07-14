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

/* Suffix trie over a fixed text. O(m) substring membership and occurrence
 * enumeration after an O(n^2) build. Bounded text length to cap memory. */

#ifndef SCL_SEARCH_SUFFIX_TRIE_H
#define SCL_SEARCH_SUFFIX_TRIE_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"
#include "scl_stdbool.h"

/*
 * scl_suffix_trie — an index over one text supporting fast substring queries.
 *
 * Build once, then answer "does substring P occur?" in O(|P|) and "where does P
 * occur?" by enumerating every start offset, independent of the text length.
 * Each query byte is matched against a node's children (a small sibling list),
 * never against the whole text, so query cost depends only on the pattern.
 *
 * ── Security / resource bounds ───────────────────────────────────────────────
 * A suffix trie holds one node per distinct suffix prefix, i.e. up to
 * n*(n+1)/2 nodes for a text of length n — quadratic in the worst case. To keep
 * memory bounded against hostile/large input, build() rejects any text longer
 * than `max_len` (or SCL_SUFFIX_TRIE_DEFAULT_MAX when max_len == 0). For long
 * texts prefer the linear-space matchers (Boyer-Moore / KMP). All allocations
 * go through the supplied allocator; teardown is iterative (no recursion depth
 * tied to the text); every size computation is overflow-checked.
 *
 * Each node caches the number of leaves in its subtree, so count() is O(|P|)
 * with no traversal.
 */

#ifndef SCL_SUFFIX_TRIE_DEFAULT_MAX
#define SCL_SUFFIX_TRIE_DEFAULT_MAX 4096u
#endif

/* Callback invoked once per occurrence with the 0-based start offset. Return
 * false to stop enumeration early. Shared with the other string matchers. */
#ifndef SCL_SEARCH_MATCH_CB_DEFINED
#define SCL_SEARCH_MATCH_CB_DEFINED
typedef bool (*scl_search_match_cb)(size_t pos, void *user);
#endif

typedef struct scl_suffix_trie scl_suffix_trie_t;

/* Build a suffix trie over text[0..len). `text` need not outlive the trie (its
 * bytes are encoded into the edges; only offsets are returned by queries).
 * Rejects len > max_len (0 => SCL_SUFFIX_TRIE_DEFAULT_MAX). */
scl_error_t scl_suffix_trie_build(scl_allocator_t *alloc,
                                  scl_suffix_trie_t **out, const char *text,
                                  size_t len, size_t max_len) SCL_WARN_UNUSED;

/* True if pat[0..plen) occurs in the text. An empty pattern (plen==0) is a
 * substring of any text and returns true. */
bool scl_suffix_trie_contains(const scl_suffix_trie_t *trie, const char *pat,
                              size_t plen);

/* Number of occurrences (including overlapping) of pat in the text. O(|pat|).
 */
size_t scl_suffix_trie_count(const scl_suffix_trie_t *trie, const char *pat,
                             size_t plen);

/* Enumerate every start offset where pat occurs via cb (cb may be NULL to only
 * count). *out_count (optional) receives the number of occurrences. Returns
 * SCL_OK if at least one occurrence, SCL_ERR_NOT_FOUND if none, or an error. */
scl_error_t scl_suffix_trie_find_all(const scl_suffix_trie_t *trie,
                                     const char *pat, size_t plen,
                                     scl_search_match_cb cb, void *user,
                                     size_t *out_count) SCL_WARN_UNUSED;

void scl_suffix_trie_destroy(scl_suffix_trie_t *trie);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
