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

/* Thread-safe btree data structure. Guarded by scl_spinlock_t. */

#include "scl_concurrent_btree.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static scl_concurrent_btree_node_t *create_node(scl_allocator_t *alloc,
                                                bool leaf, int t,
                                                size_t key_size,
                                                size_t value_size) {
  scl_concurrent_btree_node_t *n = scl_alloc(
      alloc, sizeof(scl_concurrent_btree_node_t), alignof(max_align_t));
  if (!n)
    return NULL;
  n->keys = scl_calloc(alloc, 2 * t - 1, sizeof(void *), alignof(max_align_t));
  n->values =
      scl_calloc(alloc, 2 * t - 1, sizeof(void *), alignof(max_align_t));
  n->children = scl_calloc(alloc, 2 * t, sizeof(scl_concurrent_btree_node_t *),
                           alignof(max_align_t));
  if (scl_unlikely(!n->keys || !n->values || !n->children)) {
    scl_free(alloc, n->keys);
    scl_free(alloc, n->values);
    scl_free(alloc, n->children);
    scl_free(alloc, n);
    return NULL;
  }
  for (size_t i = 0; i < (size_t)(2 * t - 1); i++) {
    n->keys[i] = scl_alloc(alloc, key_size, alignof(max_align_t));
    n->values[i] = scl_alloc(alloc, value_size, alignof(max_align_t));
  }
  n->count = 0;
  n->leaf = leaf;
  return n;
}

void scl_cbtree_destroy(scl_allocator_t *alloc, scl_concurrent_btree_t *tree) {
  if (scl_unlikely(!tree || !tree->root))
    return;

  scl_concurrent_btree_node_t *stack[256];
  int sp = 0;
  stack[sp++] = tree->root;
  scl_concurrent_btree_node_t *stack2[256];
  int sp2 = 0;

  while (sp > 0) {
    scl_concurrent_btree_node_t *node = stack[--sp];
    stack2[sp2++] = node;
    if (!node->leaf) {
      for (size_t i = 0; i <= node->count; i++)
        if (node->children[i])
          stack[sp++] = node->children[i];
    }
  }

  while (sp2 > 0) {
    scl_concurrent_btree_node_t *node = stack2[--sp2];
    size_t max_keys = 2 * tree->t - 1;
    for (size_t i = 0; i < max_keys; i++) {
      scl_free(alloc, node->keys[i]);
      scl_free(alloc, node->values[i]);
    }
    scl_free(alloc, node->keys);
    scl_free(alloc, node->values);
    scl_free(alloc, node->children);
    scl_free(alloc, node);
  }

  tree->root = NULL;
  atomic_store_explicit(&tree->count, 0, memory_order_relaxed);
}

static void split_child(scl_allocator_t *alloc, scl_concurrent_btree_node_t *x,
                        int i, int t, size_t key_size, size_t value_size) {
  scl_concurrent_btree_node_t *y = x->children[i];
  scl_concurrent_btree_node_t *z =
      create_node(alloc, y->leaf, t, key_size, value_size);
  z->count = t - 1;
  for (int j = 0; j < t - 1; j++) {
    scl_memcpy(z->keys[j], y->keys[j + t], key_size);
    scl_memcpy(z->values[j], y->values[j + t], value_size);
  }
  if (!y->leaf) {
    for (int j = 0; j < t; j++)
      z->children[j] = y->children[j + t];
  }
  y->count = t - 1;
  for (int j = (int)x->count; j >= i + 1; j--)
    x->children[j + 1] = x->children[j];
  x->children[i + 1] = z;
  for (int j = (int)x->count - 1; j >= i; j--) {
    scl_memcpy(x->keys[j + 1], x->keys[j], key_size);
    scl_memcpy(x->values[j + 1], x->values[j], value_size);
  }
  scl_memcpy(x->keys[i], y->keys[t - 1], key_size);
  scl_memcpy(x->values[i], y->values[t - 1], value_size);
  x->count++;
}

scl_error_t scl_cbtree_init(scl_allocator_t *alloc,
                            scl_concurrent_btree_t *tree, size_t key_size,
                            size_t value_size, int degree, scl_cmp_func_t cmp) {
  if (scl_unlikely(!tree))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(key_size == 0 || value_size == 0 || degree < 2 || !cmp))
    return SCL_ERR_INVALID_ARG;
  tree->t = degree;
  tree->root = create_node(alloc, true, degree, key_size, value_size);
  if (scl_unlikely(!tree->root))
    return SCL_ERR_OUT_OF_MEMORY;
  tree->key_size = key_size;
  tree->value_size = value_size;
  atomic_init(&tree->count, 0);
  tree->cmp = cmp;
  scl_spinlock_init(&tree->lock);
  return SCL_OK;
}

scl_error_t scl_cbtree_insert(scl_allocator_t *alloc,
                              scl_concurrent_btree_t *tree,
                              const void *SCL_RESTRICT key, const void *value) {
  if (scl_unlikely(!tree || !key || !value))
    return SCL_ERR_NULL_PTR;
  scl_spinlock_lock(&tree->lock);

  if (tree->root->count == (size_t)(2 * tree->t - 1)) {
    scl_concurrent_btree_node_t *s =
        create_node(alloc, false, tree->t, tree->key_size, tree->value_size);
    if (!s) {
      scl_spinlock_unlock(&tree->lock);
      return SCL_ERR_OUT_OF_MEMORY;
    }
    s->children[0] = tree->root;
    tree->root = s;
    split_child(alloc, s, 0, tree->t, tree->key_size, tree->value_size);
  }

  scl_concurrent_btree_node_t *node = tree->root;
  bool inserted = false;

  while (1) {
    int i = (int)node->count - 1;
    if (node->leaf) {
      while (i >= 0 && tree->cmp(key, node->keys[i]) < 0) {
        scl_memcpy(node->keys[i + 1], node->keys[i], tree->key_size);
        scl_memcpy(node->values[i + 1], node->values[i], tree->value_size);
        i--;
      }
      i++;
      if (i < (int)node->count && tree->cmp(key, node->keys[i]) == 0) {
        scl_memcpy(node->values[i], value, tree->value_size);
        scl_spinlock_unlock(&tree->lock);
        return SCL_OK;
      }
      scl_memcpy(node->keys[i], key, tree->key_size);
      scl_memcpy(node->values[i], value, tree->value_size);
      node->count++;
      inserted = true;
      break;
    } else {
      while (i >= 0 && tree->cmp(key, node->keys[i]) < 0)
        i--;
      i++;
      if (i < (int)node->count && tree->cmp(key, node->keys[i]) == 0) {
        scl_memcpy(node->values[i], value, tree->value_size);
        scl_spinlock_unlock(&tree->lock);
        return SCL_OK;
      }
      if (node->children[i]->count == (size_t)(2 * tree->t - 1)) {
        split_child(alloc, node, i, tree->t, tree->key_size, tree->value_size);
        if (tree->cmp(key, node->keys[i]) > 0)
          i++;
      }
      node = node->children[i];
    }
  }

  if (inserted)
    atomic_fetch_add_explicit(&tree->count, 1, memory_order_relaxed);
  scl_spinlock_unlock(&tree->lock);
  return SCL_OK;
}

scl_error_t scl_cbtree_get(scl_concurrent_btree_t *tree, const void *key,
                           void *SCL_RESTRICT out_value) {
  if (scl_unlikely(!tree || !key || !out_value))
    return SCL_ERR_NULL_PTR;
  scl_spinlock_lock(&tree->lock);

  scl_concurrent_btree_node_t *node = tree->root;
  while (scl_likely(node)) {
    int i = 0;
    while (i < (int)node->count && tree->cmp(key, node->keys[i]) > 0)
      i++;
    if (i < (int)node->count && tree->cmp(key, node->keys[i]) == 0) {
      scl_memcpy(out_value, node->values[i], tree->value_size);
      scl_spinlock_unlock(&tree->lock);
      return SCL_OK;
    }
    if (node->leaf)
      break;
    node = node->children[i];
  }
  scl_spinlock_unlock(&tree->lock);
  return SCL_ERR_NOT_FOUND;
}

bool scl_cbtree_contains(scl_concurrent_btree_t *tree, const void *key) {
  if (scl_unlikely(!tree || !key))
    return false;
  scl_spinlock_lock(&tree->lock);
  scl_concurrent_btree_node_t *node = tree->root;
  while (scl_likely(node)) {
    int i = 0;
    while (i < (int)node->count && tree->cmp(key, node->keys[i]) > 0)
      i++;
    if (i < (int)node->count && tree->cmp(key, node->keys[i]) == 0) {
      scl_spinlock_unlock(&tree->lock);
      return true;
    }
    if (node->leaf)
      break;
    node = node->children[i];
  }
  scl_spinlock_unlock(&tree->lock);
  return false;
}

scl_error_t scl_cbtree_remove(scl_allocator_t *alloc,
                              scl_concurrent_btree_t *tree,
                              const void *SCL_RESTRICT key) {
  (void)alloc;
  (void)tree;
  (void)key;
  return SCL_ERR_NOT_IMPLEMENTED;
}

size_t scl_cbtree_count(const scl_concurrent_btree_t *tree) {
  return tree ? atomic_load_explicit(&tree->count, memory_order_relaxed) : 0;
}
