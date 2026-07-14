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

/* Trie search. O(k) per lookup. Prefix-tree for dictionary/autocomplete. */

#include "scl_trie_search.h"
#include "scl_string.h"

static scl_search_trie_node_t *node_create(scl_search_trie_t *trie) {
  if (scl_unlikely(trie->node_count >= trie->node_cap))
    return NULL;
  scl_search_trie_node_t *n = (scl_search_trie_node_t *)scl_calloc(
      trie->alloc, 1, sizeof(scl_search_trie_node_t), alignof(max_align_t));
  if (n) {
    n->is_end = false;
    trie->node_count++;
  }
  return n;
}

static bool node_has_children(const scl_search_trie_node_t *node) {
  for (int i = 0; i < SCL_SEARCH_TRIE_ALPHABET_SIZE; i++)
    if (node->children[i])
      return true;
  return false;
}

scl_error_t scl_search_trie_init(scl_allocator_t *alloc,
                                 scl_search_trie_t **SCL_RESTRICT trie) {
  if (scl_unlikely(trie == NULL))
    return SCL_ERR_NULL_PTR;
  scl_search_trie_t *t = (scl_search_trie_t *)scl_alloc(
      alloc, sizeof(scl_search_trie_t), alignof(max_align_t));
  if (scl_unlikely(t == NULL))
    return SCL_ERR_OUT_OF_MEMORY;
  t->root = node_create(t);
  if (!t->root) {
    scl_free(alloc, t);
    return SCL_ERR_OUT_OF_MEMORY;
  }
  t->alloc = alloc;
  t->node_count = 0;
  t->node_cap = SCL_SEARCH_TRIE_MAX_NODES;
  *trie = t;
  return SCL_OK;
}

scl_error_t scl_search_trie_insert(scl_search_trie_t *trie, const char *word) {
  if (scl_unlikely(trie == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(word == NULL))
    return SCL_ERR_NULL_PTR;
  scl_search_trie_node_t *node = trie->root;
  while (*word) {
    unsigned char c = (unsigned char)*word;
    if (!node->children[c]) {
      node->children[c] = node_create(trie);
      if (!node->children[c])
        return SCL_ERR_OUT_OF_MEMORY;
    }
    node = node->children[c];
    word++;
  }
  node->is_end = true;
  return SCL_OK;
}

bool scl_search_trie_search(const scl_search_trie_t *trie, const char *word) {
  if (!trie || !word)
    return false;
  scl_search_trie_node_t *node = trie->root;
  while (*word) {
    unsigned char c = (unsigned char)*word;
    if (!node->children[c])
      return false;
    node = node->children[c];
    word++;
  }
  return node->is_end;
}

bool scl_search_trie_starts_with(const scl_search_trie_t *trie,
                                 const char *prefix) {
  if (!trie || !prefix)
    return false;
  scl_search_trie_node_t *node = trie->root;
  while (*prefix) {
    unsigned char c = (unsigned char)*prefix;
    if (!node->children[c])
      return false;
    node = node->children[c];
    prefix++;
  }
  return true;
}

typedef struct {
  scl_search_trie_node_t *node;
  const char *word;
} trie_del_frame_t;

scl_error_t scl_search_trie_delete(scl_search_trie_t *trie, const char *word) {
  if (scl_unlikely(trie == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(word == NULL))
    return SCL_ERR_NULL_PTR;
  if (!trie->root)
    return SCL_ERR_NOT_FOUND;
  if (!scl_search_trie_search(trie, word))
    return SCL_ERR_NOT_FOUND;

  trie_del_frame_t stack[1024];
  int sp = 0;
  scl_search_trie_node_t *node = trie->root;
  const char *w = word;

  while (*w) {
    unsigned char c = (unsigned char)*w;
    stack[sp].node = node;
    stack[sp].word = w;
    sp++;
    node = node->children[c];
    w++;
  }
  node->is_end = false;

  while (sp > 0) {
    sp--;
    scl_search_trie_node_t *parent = stack[sp].node;
    unsigned char c = (unsigned char)stack[sp].word[0];
    if (!node_has_children(node) && !node->is_end) {
      scl_free(trie->alloc, node);
      parent->children[c] = NULL;
    }
    node = parent;
  }

  return SCL_OK;
}

void scl_search_trie_destroy(scl_search_trie_t *trie) {
  if (!trie)
    return;
  if (!trie->root) {
    scl_free(trie->alloc, trie);
    return;
  }

  scl_search_trie_node_t **stack = (scl_search_trie_node_t **)scl_alloc(
      trie->alloc, 4096 * sizeof(scl_search_trie_node_t *),
      alignof(max_align_t));
  if (!stack)
    return;

  int sp = 0;
  stack[sp++] = trie->root;

  while (sp > 0) {
    scl_search_trie_node_t *node = stack[--sp];
    if (!node)
      continue;
    for (int i = 0; i < SCL_SEARCH_TRIE_ALPHABET_SIZE; i++) {
      if (node->children[i]) {
        if (sp >= 4095)
          break;
        stack[sp++] = node->children[i];
      }
    }
    scl_free(trie->alloc, node);
  }

  scl_free(trie->alloc, stack);
  scl_free(trie->alloc, trie);
}
