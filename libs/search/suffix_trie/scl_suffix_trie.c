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

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O2")
#endif

#include "scl_suffix_trie.h"

/*
 * Nodes use a first-child / next-sibling layout (not a 256-way array) so each
 * node is small — a 256-pointer-per-node trie over n suffixes would be
 * catastrophic for memory. `all_next` threads every node onto one list so
 * teardown is a single O(nodes) sweep with no recursion. `leaf_count` caches
 * the number of leaves in the subtree, making count() O(|pattern|).
 */
typedef struct st_node {
  struct st_node *child;    /* first child */
  struct st_node *sibling;  /* next sibling */
  struct st_node *all_next; /* intrusive list of every node, for teardown */
  size_t suffix_index;
  size_t leaf_count;  /* leaves in this subtree (occurrence count) */
  unsigned char edge; /* byte labelling the edge from the parent */
  bool is_leaf;       /* a suffix ends here */
} st_node;

struct scl_suffix_trie {
  scl_allocator_t *alloc;
  st_node *root;
  st_node *all_head;
  size_t text_len;
  size_t node_count;
};

static st_node *st_new_node(scl_suffix_trie_t *t, unsigned char edge) {
  st_node *n =
      (st_node *)scl_calloc(t->alloc, 1, sizeof(st_node), alignof(max_align_t));
  if (scl_unlikely(!n))
    return NULL;
  n->edge = edge;
  n->all_next = t->all_head;
  t->all_head = n;
  t->node_count++;
  return n;
}

static st_node *st_find_child(st_node *parent, unsigned char ch) {
  for (st_node *c = parent->child; c; c = c->sibling)
    if (c->edge == ch)
      return c;
  return NULL;
}

scl_error_t scl_suffix_trie_build(scl_allocator_t *alloc,
                                  scl_suffix_trie_t **out, const char *text,
                                  size_t len, size_t max_len) {
  if (scl_unlikely(!alloc || !out))
    return SCL_ERR_NULL_PTR;
  *out = NULL;
  if (scl_unlikely(!text && len > 0))
    return SCL_ERR_NULL_PTR;
  if (max_len == 0)
    max_len = SCL_SUFFIX_TRIE_DEFAULT_MAX;
  if (scl_unlikely(len > max_len))
    return SCL_ERR_SIZE_OVERFLOW;

  scl_suffix_trie_t *t = (scl_suffix_trie_t *)scl_calloc(alloc, 1, sizeof(*t),
                                                         alignof(max_align_t));
  if (scl_unlikely(!t))
    return SCL_ERR_OUT_OF_MEMORY;
  t->alloc = alloc;
  t->text_len = len;

  t->root = st_new_node(t, 0);
  if (scl_unlikely(!t->root)) {
    scl_suffix_trie_destroy(t);
    return SCL_ERR_OUT_OF_MEMORY;
  }

  /* Insert every suffix text[i..len). Each node on a suffix's path has its
   * leaf_count incremented, so leaf_count ends up equal to the number of
   * suffixes passing through the node = leaves in its subtree. */
  for (size_t i = 0; i < len; i++) {
    st_node *cur = t->root;
    t->root->leaf_count++;
    for (size_t k = i; k < len; k++) {
      unsigned char ch = (unsigned char)text[k];
      st_node *child = st_find_child(cur, ch);
      if (!child) {
        child = st_new_node(t, ch);
        if (scl_unlikely(!child)) {
          scl_suffix_trie_destroy(t);
          return SCL_ERR_OUT_OF_MEMORY;
        }
        child->sibling = cur->child;
        cur->child = child;
      }
      cur = child;
      cur->leaf_count++;
    }
    cur->is_leaf = true;
    cur->suffix_index = i;
  }

  *out = t;
  return SCL_OK;
}

/* Walk the pattern from the root; return the node where it ends, or NULL if any
 * byte has no matching edge (pattern absent). */
static const st_node *st_walk(const scl_suffix_trie_t *t, const char *pat,
                              size_t plen) {
  const st_node *cur = t->root;
  for (size_t k = 0; k < plen; k++) {
    unsigned char ch = (unsigned char)pat[k];
    const st_node *c = cur->child;
    while (c && c->edge != ch)
      c = c->sibling;
    if (!c)
      return NULL;
    cur = c;
  }
  return cur;
}

bool scl_suffix_trie_contains(const scl_suffix_trie_t *trie, const char *pat,
                              size_t plen) {
  if (scl_unlikely(!trie || (!pat && plen > 0)))
    return false;
  if (plen == 0)
    return true; /* empty string is a substring */
  return st_walk(trie, pat, plen) != NULL;
}

size_t scl_suffix_trie_count(const scl_suffix_trie_t *trie, const char *pat,
                             size_t plen) {
  if (scl_unlikely(!trie || (!pat && plen > 0)))
    return 0;
  if (plen == 0)
    return trie->text_len; /* occurs at every position */
  const st_node *n = st_walk(trie, pat, plen);
  return n ? n->leaf_count : 0;
}

scl_error_t scl_suffix_trie_find_all(const scl_suffix_trie_t *trie,
                                     const char *pat, size_t plen,
                                     scl_search_match_cb cb, void *user,
                                     size_t *out_count) {
  if (scl_unlikely(!trie))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(plen == 0))
    return SCL_ERR_INVALID_ARG;
  if (scl_unlikely(!pat))
    return SCL_ERR_NULL_PTR;

  const st_node *start = st_walk(trie, pat, plen);
  if (!start) {
    if (out_count)
      *out_count = 0;
    return SCL_ERR_NOT_FOUND;
  }

  /* Iterative DFS over the subtree, collecting the offset stored at each leaf.
   * Heap stack (allocator-backed) so a deep subtree cannot overflow the call
   * stack; capacity is bounded by the (already bounded) node count. */
  size_t cap = 64, top = 0;
  const st_node **stk = (const st_node **)scl_alloc(
      trie->alloc, cap * sizeof(*stk), alignof(max_align_t));
  if (scl_unlikely(!stk))
    return SCL_ERR_OUT_OF_MEMORY;

  size_t count = 0;
  bool stopped = false;
  stk[top++] = start;
  while (top > 0 && !stopped) {
    const st_node *n = stk[--top];
    if (n->is_leaf) {
      count++;
      if (cb && !cb(n->suffix_index, user)) {
        stopped = true;
        break;
      }
    }
    for (const st_node *c = n->child; c; c = c->sibling) {
      if (top == cap) {
        size_t ncap = cap * 2;
        size_t nbytes;
        if (scl_unlikely(scl_mul_overflow(ncap, sizeof(*stk), &nbytes))) {
          scl_free(trie->alloc, stk);
          return SCL_ERR_SIZE_OVERFLOW;
        }
        const st_node **ns = (const st_node **)scl_realloc(
            trie->alloc, stk, cap * sizeof(*stk), nbytes, alignof(max_align_t));
        if (scl_unlikely(!ns)) {
          scl_free(trie->alloc, stk);
          return SCL_ERR_OUT_OF_MEMORY;
        }
        stk = ns;
        cap = ncap;
      }
      stk[top++] = c;
    }
  }

  scl_free(trie->alloc, stk);
  if (out_count)
    *out_count = count;
  return count > 0 ? SCL_OK : SCL_ERR_NOT_FOUND;
}

void scl_suffix_trie_destroy(scl_suffix_trie_t *trie) {
  if (!trie)
    return;
  st_node *n = trie->all_head;
  while (n) {
    st_node *next = n->all_next;
    scl_free(trie->alloc, n);
    n = next;
  }
  scl_free(trie->alloc, trie);
}
