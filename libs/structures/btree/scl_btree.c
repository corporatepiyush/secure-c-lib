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

/* btree data structure. */

#include "scl_btree.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static scl_btree_node_t *scl_btree_create_node(scl_allocator_t *alloc,
                                               bool leaf, int t, size_t ksz,
                                               size_t vsz) {
  size_t maxk = (size_t)(2 * t - 1);
  size_t maxc = (size_t)(2 * t);
  size_t sz = sizeof(scl_btree_node_t) + ksz * maxk + vsz * maxk +
              sizeof(scl_btree_node_t *) * maxc;
  scl_btree_node_t *node = scl_alloc(alloc, sz, alignof(max_align_t));
  if (!node)
    return NULL;
  scl_memset(node, 0, sz);
  node->count = 0;
  node->leaf = leaf;
  return node;
}

void scl_btree_destroy(scl_allocator_t *alloc, scl_btree_t *tree) {
  if (scl_unlikely(!tree || !tree->root))
    return;

  scl_btree_node_t *stack[256];
  int sp = 0;
  stack[sp++] = tree->root;

  while (sp > 0) {
    scl_btree_node_t *node = stack[--sp];
    if (!node->leaf) {
      size_t maxk = (size_t)(2 * tree->t - 1);
      scl_btree_node_t **ch =
          scl_btree_node_children(node, tree->key_size, tree->value_size, maxk);
      for (size_t i = 0; i <= node->count; i++)
        if (ch[i])
          stack[sp++] = ch[i];
    }
    scl_free(alloc, node);
  }

  tree->root = NULL;
  tree->count = 0;
}

scl_error_t scl_btree_init(scl_allocator_t *alloc, scl_btree_t *tree,
                           size_t key_size, size_t value_size, int degree,
                           scl_cmp_func_t cmp) {
  if (scl_unlikely(!tree || !cmp))
    return SCL_ERR_NULL_PTR;
  if (key_size == 0 || value_size == 0 || degree < 2)
    return SCL_ERR_INVALID_ARG;

  tree->root = scl_btree_create_node(alloc, true, degree, key_size, value_size);
  if (scl_unlikely(!tree->root))
    return SCL_ERR_OUT_OF_MEMORY;

  tree->key_size = key_size;
  tree->value_size = value_size;
  tree->count = 0;
  tree->cmp = cmp;
  tree->t = degree;
  return SCL_OK;
}

static void scl_btree_split_child(scl_allocator_t *alloc,
                                  scl_btree_node_t *parent, size_t i, int t,
                                  size_t ksz, size_t vsz) {
  size_t maxk = (size_t)(2 * t - 1);
  scl_btree_node_t **pch = scl_btree_node_children(parent, ksz, vsz, maxk);
  scl_btree_node_t *child = pch[i];
  scl_btree_node_t *new_child =
      scl_btree_create_node(alloc, child->leaf, t, ksz, vsz);
  if (scl_unlikely(!new_child))
    return;

  unsigned char *ck = scl_btree_node_keys(child);
  unsigned char *cv = scl_btree_node_vals(child, ksz, maxk);
  unsigned char *nk = scl_btree_node_keys(new_child);
  unsigned char *nv = scl_btree_node_vals(new_child, ksz, maxk);
  scl_btree_node_t **cch = scl_btree_node_children(child, ksz, vsz, maxk);
  scl_btree_node_t **nch = scl_btree_node_children(new_child, ksz, vsz, maxk);

  new_child->count = (size_t)(t - 1);
  for (int j = 0; j < t - 1; j++) {
    scl_memcpy(nk + (size_t)j * ksz, ck + (size_t)(j + t) * ksz, ksz);
    scl_memcpy(nv + (size_t)j * vsz, cv + (size_t)(j + t) * vsz, vsz);
  }

  if (!child->leaf) {
    for (int j = 0; j < t; j++)
      nch[j] = cch[j + t];
  }

  child->count = (size_t)(t - 1);

  for (size_t j = parent->count; j > i; j--)
    pch[j + 1] = pch[j];
  pch[i + 1] = new_child;

  unsigned char *pk = scl_btree_node_keys(parent);
  unsigned char *pv = scl_btree_node_vals(parent, ksz, maxk);
  if (parent->count > i) {
    size_t shift_cnt = parent->count - i;
    scl_memmove(pk + (i + 1) * ksz, pk + i * ksz, shift_cnt * ksz);
    scl_memmove(pv + (i + 1) * vsz, pv + i * vsz, shift_cnt * vsz);
  }
  scl_memcpy(pk + i * ksz, ck + (size_t)(t - 1) * ksz, ksz);
  scl_memcpy(pv + i * vsz, cv + (size_t)(t - 1) * vsz, vsz);
  parent->count++;
}

scl_error_t scl_btree_insert(scl_allocator_t *alloc, scl_btree_t *tree,
                             const void *SCL_RESTRICT key, const void *value) {
  if (scl_unlikely(!tree || !key || !value))
    return SCL_ERR_NULL_PTR;

  int t = tree->t;
  size_t ksz = tree->key_size;
  size_t vsz = tree->value_size;
  size_t maxk = (size_t)(2 * t - 1);
  scl_cmp_func_t cmp = tree->cmp;

  if (tree->root->count == maxk) {
    scl_btree_node_t *new_root =
        scl_btree_create_node(alloc, false, t, ksz, vsz);
    if (scl_unlikely(!new_root))
      return SCL_ERR_OUT_OF_MEMORY;
    scl_btree_node_t **nrch = scl_btree_node_children(new_root, ksz, vsz, maxk);
    nrch[0] = tree->root;
    tree->root = new_root;
    scl_btree_split_child(alloc, new_root, 0, t, ksz, vsz);
  }

scl_btree_node_t *node = tree->root;

   for (;;) {
     unsigned char *nk = scl_btree_node_keys(node);
     unsigned char *nv = scl_btree_node_vals(node, ksz, maxk);

     if (node->leaf) {
       size_t i = node->count;
       while (i > 0 && cmp(key, nk + (i - 1) * ksz) < 0) {
         i--;
       }
       /* Shift keys/values right in bulk via memmove */
       if (i < node->count) {
         size_t shift_cnt = node->count - i;
         scl_memmove(nk + (i + 1) * ksz, nk + i * ksz,
                     shift_cnt * ksz);
         scl_memmove(nv + (i + 1) * vsz, nv + i * vsz,
                     shift_cnt * vsz);
       }
       if (i < node->count && cmp(key, nk + i * ksz) == 0) {
         scl_memcpy(nv + i * vsz, value, vsz);
         return SCL_OK;
       }
       scl_memcpy(nk + i * ksz, key, ksz);
       scl_memcpy(nv + i * vsz, value, vsz);
       node->count++;
       break;
     } else {
       size_t i = node->count;
       while (i > 0 && cmp(key, nk + (i - 1) * ksz) < 0)
         i--;
       if (i < node->count && cmp(key, nk + i * ksz) == 0) {
         scl_memcpy(nv + i * vsz, value, vsz);
         return SCL_OK;
       }
       scl_btree_node_t **ch = scl_btree_node_children(node, ksz, vsz, maxk);
       if (ch[i]->count == maxk) {
         scl_btree_split_child(alloc, node, i, t, ksz, vsz);
         if (cmp(key, scl_btree_node_keys(node) + i * ksz) > 0)
           i++;
       }
       node = scl_btree_node_children(node, ksz, vsz, maxk)[i];
     }
   }

  tree->count++;
  return SCL_OK;
}

scl_error_t scl_btree_get(const scl_btree_t *tree, const void *key,
                           void *SCL_RESTRICT out_value) {
   if (scl_unlikely(!tree || !key || !out_value))
     return SCL_ERR_NULL_PTR;

   int t = tree->t;
   size_t ksz = tree->key_size;
   size_t vsz = tree->value_size;
   size_t maxk = (size_t)(2 * t - 1);
   scl_cmp_func_t cmp = tree->cmp;

   scl_btree_node_t *node = tree->root;
   while (scl_likely(node)) {
     unsigned char *nk = scl_btree_node_keys(node);
     unsigned char *nv = scl_btree_node_vals(node, ksz, maxk);

     size_t lo = 0, hi = node->count;
     while (lo < hi) {
       size_t mid = lo + (hi - lo) / 2;
       int r = cmp(key, nk + mid * ksz);
       if (r == 0) {
         scl_memcpy(out_value, nv + mid * vsz, vsz);
         return SCL_OK;
       }
       if (r < 0)
         hi = mid;
       else
         lo = mid + 1;
     }
     if (node->leaf)
       break;
     scl_btree_node_t **ch = scl_btree_node_children(node, ksz, vsz, maxk);
     node = ch[lo];
   }
   return SCL_ERR_NOT_FOUND;
 }

 bool scl_btree_contains(const scl_btree_t *tree, const void *key) {
   if (scl_unlikely(!tree || !key))
     return false;

int t = tree->t;
    size_t ksz = tree->key_size;
    size_t maxk = (size_t)(2 * t - 1);
    scl_cmp_func_t cmp = tree->cmp;

   scl_btree_node_t *node = tree->root;
  while (scl_likely(node)) {
    unsigned char *nk = scl_btree_node_keys(node);

    size_t lo = 0, hi = node->count;
    while (lo < hi) {
      size_t mid = lo + (hi - lo) / 2;
      int r = cmp(key, nk + mid * ksz);
      if (r == 0)
        return true;
      if (r < 0)
        hi = mid;
      else
        lo = mid + 1;
    }
    if (node->leaf)
      break;
    scl_btree_node_t **ch =
        scl_btree_node_children(node, ksz, tree->value_size, maxk);
    node = ch[lo];
  }
  return false;
}

/* ── B-tree remove: proactive merge/borrow ──────────────────────── */

/* Find the first index i where key <= node->keys[i].
   Returns an index in [0, node->count] where key belongs. */
static int scl_btree_find_key(const scl_btree_t *tree,
                              const scl_btree_node_t *node, const void *key,
                              unsigned char *nk, size_t maxk) {
  (void)maxk;
  int lo = 0, hi = (int)node->count;
  while (lo < hi) {
    int mid = lo + (hi - lo) / 2;
    int r = tree->cmp(key, nk + (size_t)mid * tree->key_size);
    if (r == 0)
      return mid;
    if (r < 0)
      hi = mid;
    else
      lo = mid + 1;
  }
  return lo;
}

/* Borrow a key from the left sibling when node->children[idx] has t-1 keys.
   The borrowed key comes via the parent key at position idx-1. */
static void scl_btree_borrow_from_left(scl_btree_node_t *parent, int idx, int t,
                                       size_t ksz, size_t vsz, size_t maxk) {
  (void)t;
  scl_btree_node_t *child =
      scl_btree_node_children(parent, ksz, vsz, maxk)[(size_t)idx];
  scl_btree_node_t *left =
      scl_btree_node_children(parent, ksz, vsz, maxk)[(size_t)(idx - 1)];

  unsigned char *ck = scl_btree_node_keys(child);
  unsigned char *cv = scl_btree_node_vals(child, ksz, maxk);
  unsigned char *lk = scl_btree_node_keys(left);
  unsigned char *lv = scl_btree_node_vals(left, ksz, maxk);
  unsigned char *pk = scl_btree_node_keys(parent);
  unsigned char *pv = scl_btree_node_vals(parent, ksz, maxk);
  scl_btree_node_t **cch = scl_btree_node_children(child, ksz, vsz, maxk);
  scl_btree_node_t **lch = scl_btree_node_children(left, ksz, vsz, maxk);

  /* Shift child's keys/values/children right by 1 */
  if (child->count > 0) {
    scl_memmove(ck + ksz, ck, child->count * ksz);
    scl_memmove(cv + vsz, cv, child->count * vsz);
  }
  if (!child->leaf) {
    for (size_t i = child->count + 1; i > 0; i--)
      cch[i] = cch[i - 1];
  }

  /* Move parent key (idx-1) down to child[0] */
  scl_memcpy(ck, pk + (size_t)(idx - 1) * ksz, ksz);
  scl_memcpy(cv, pv + (size_t)(idx - 1) * vsz, vsz);

  /* Move left's last key up to parent */
  scl_memcpy(pk + (size_t)(idx - 1) * ksz, lk + (size_t)(left->count - 1) * ksz,
             ksz);
  scl_memcpy(pv + (size_t)(idx - 1) * vsz, lv + (size_t)(left->count - 1) * vsz,
             vsz);

  /* Move left's last child to child[0] */
  if (!child->leaf) {
    cch[0] = lch[left->count];
  }

  child->count++;
  left->count--;
}

/* Borrow a key from the right sibling when node->children[idx] has t-1 keys. */
static void scl_btree_borrow_from_right(scl_btree_node_t *parent, int idx,
                                        int t, size_t ksz, size_t vsz,
                                        size_t maxk) {
  (void)t;
  scl_btree_node_t *child =
      scl_btree_node_children(parent, ksz, vsz, maxk)[(size_t)idx];
  scl_btree_node_t *right =
      scl_btree_node_children(parent, ksz, vsz, maxk)[(size_t)(idx + 1)];

  unsigned char *ck = scl_btree_node_keys(child);
  unsigned char *cv = scl_btree_node_vals(child, ksz, maxk);
  unsigned char *rk = scl_btree_node_keys(right);
  unsigned char *rv = scl_btree_node_vals(right, ksz, maxk);
  unsigned char *pk = scl_btree_node_keys(parent);
  unsigned char *pv = scl_btree_node_vals(parent, ksz, maxk);
  scl_btree_node_t **cch = scl_btree_node_children(child, ksz, vsz, maxk);
  scl_btree_node_t **rch = scl_btree_node_children(right, ksz, vsz, maxk);

  /* Move parent key idx down to child */
  scl_memcpy(ck + (size_t)child->count * ksz, pk + (size_t)idx * ksz, ksz);
  scl_memcpy(cv + (size_t)child->count * vsz, pv + (size_t)idx * vsz, vsz);

  /* Move right's first child to child */
  if (!child->leaf)
    cch[child->count + 1] = rch[0];

  /* Move right's first key up to parent */
  scl_memcpy(pk + (size_t)idx * ksz, rk, ksz);
  scl_memcpy(pv + (size_t)idx * vsz, rv, vsz);

  /* Shift right's keys/values left by 1 */
  if (right->count > 1) {
    scl_memmove(rk, rk + ksz, (right->count - 1) * ksz);
    scl_memmove(rv, rv + vsz, (right->count - 1) * vsz);
  }
  if (!right->leaf) {
    for (size_t i = 0; i < right->count; i++)
      rch[i] = rch[i + 1];
  }

  child->count++;
  right->count--;
}

/* Merge child[idx] and child[idx+1] via the parent key at idx.
   After merge, the parent loses one key and one child pointer. */
static void scl_btree_merge(scl_allocator_t *alloc, scl_btree_node_t *parent,
                            int idx, int t, size_t ksz, size_t vsz,
                            size_t maxk) {
  (void)t;
  scl_btree_node_t **ch = scl_btree_node_children(parent, ksz, vsz, maxk);
  scl_btree_node_t *left = ch[(size_t)idx];
  scl_btree_node_t *right = ch[(size_t)(idx + 1)];

  unsigned char *lk = scl_btree_node_keys(left);
  unsigned char *lv = scl_btree_node_vals(left, ksz, maxk);
  unsigned char *rk = scl_btree_node_keys(right);
  unsigned char *rv = scl_btree_node_vals(right, ksz, maxk);
  unsigned char *pk = scl_btree_node_keys(parent);
  unsigned char *pv = scl_btree_node_vals(parent, ksz, maxk);
  scl_btree_node_t **lch = scl_btree_node_children(left, ksz, vsz, maxk);
  scl_btree_node_t **rch = scl_btree_node_children(right, ksz, vsz, maxk);

  /* Pull parent key idx down into left */
  scl_memcpy(lk + (size_t)left->count * ksz, pk + (size_t)idx * ksz, ksz);
  scl_memcpy(lv + (size_t)left->count * vsz, pv + (size_t)idx * vsz, vsz);
  left->count++;

  /* Copy all of right's keys/values into left */
  scl_memcpy(lk + (size_t)left->count * ksz, rk, (size_t)right->count * ksz);
  scl_memcpy(lv + (size_t)left->count * vsz, rv, (size_t)right->count * vsz);

  /* Copy all of right's children into left */
  if (!left->leaf) {
    for (size_t i = 0; i <= right->count; i++)
      lch[left->count + i] = rch[i];
  }
  left->count += right->count;

  /* Shift parent keys/children left past idx */
  for (size_t i = (size_t)idx; i < parent->count - 1; i++) {
    scl_memcpy(pk + i * ksz, pk + (i + 1) * ksz, ksz);
    scl_memcpy(pv + i * vsz, pv + (i + 1) * vsz, vsz);
  }
  for (size_t i = (size_t)(idx + 1); i < parent->count; i++)
    ch[i] = ch[i + 1];

  parent->count--;

  scl_free(alloc, right);
}

/* Get the maximum key from the subtree rooted at node (predecessor).
   Returns the key pointer into the node's flat array. */
static unsigned char *scl_btree_max_key(scl_btree_node_t *node, size_t ksz,
                                        size_t vsz, size_t maxk) {
  while (!node->leaf) {
    scl_btree_node_t **ch = scl_btree_node_children(node, ksz, vsz, maxk);
    node = ch[node->count];
  }
  unsigned char *nk = scl_btree_node_keys(node);
  return nk + (node->count - 1) * ksz;
}

/* Get the maximum value from the subtree rooted at node, paired with max_key.
 */
static unsigned char *scl_btree_max_value(scl_btree_node_t *node, size_t ksz,
                                          size_t vsz, size_t maxk) {
  while (!node->leaf) {
    scl_btree_node_t **ch = scl_btree_node_children(node, ksz, vsz, maxk);
    node = ch[node->count];
  }
  unsigned char *nv = scl_btree_node_vals(node, ksz, maxk);
  return nv + (node->count - 1) * vsz;
}

/* Recursive remove: ensures the node has at least t keys before descending
   (proactive merge/borrow). Returns true if the key was removed. */
static bool scl_btree_remove_recursive(scl_allocator_t *alloc,
                                       scl_btree_t *tree,
                                       scl_btree_node_t *node, const void *key,
                                       int t, size_t ksz, size_t vsz,
                                       size_t maxk) {
  unsigned char *nk = scl_btree_node_keys(node);
  unsigned char *nv = scl_btree_node_vals(node, ksz, maxk);
  scl_btree_node_t **ch = scl_btree_node_children(node, ksz, vsz, maxk);
  scl_cmp_func_t cmp = tree->cmp;

  int idx = scl_btree_find_key(tree, node, key, nk, maxk);

  if (idx < (int)node->count && cmp(key, nk + (size_t)idx * ksz) == 0) {
    /* Case 1: key found in this node */
    if (node->leaf) {
      /* Leaf: simply remove — bulk memmove for remaining keys/values */
      if ((size_t)idx < node->count - 1) {
        size_t tail_cnt = node->count - 1 - (size_t)idx;
        scl_memmove(nk + (size_t)idx * ksz, nk + ((size_t)idx + 1) * ksz,
                    tail_cnt * ksz);
        scl_memmove(nv + (size_t)idx * vsz, nv + ((size_t)idx + 1) * vsz,
                    tail_cnt * vsz);
      }
      node->count--;
      return true;
    } else {
      /* Internal node: find predecessor or successor to replace */
      if (ch[idx]->count >= (size_t)t) {
        /* Case 2a: left child has >= t keys, use predecessor */
        unsigned char *pred_key = scl_btree_max_key(ch[idx], ksz, vsz, maxk);
        unsigned char *pred_val = scl_btree_max_value(ch[idx], ksz, vsz, maxk);
        scl_memcpy(nk + (size_t)idx * ksz, pred_key, ksz);
        scl_memcpy(nv + (size_t)idx * vsz, pred_val, vsz);
        return scl_btree_remove_recursive(alloc, tree, ch[idx], pred_key, t,
                                          ksz, vsz, maxk);
      } else if (ch[idx + 1]->count >= (size_t)t) {
        /* Case 2b: right child has >= t keys, use successor */
        /* Find successor: min of right subtree */
        scl_btree_node_t *succ_node = ch[idx + 1];
        while (!succ_node->leaf) {
          scl_btree_node_t **sch =
              scl_btree_node_children(succ_node, ksz, vsz, maxk);
          succ_node = sch[0];
        }
        unsigned char *sk = scl_btree_node_keys(succ_node);
        unsigned char *sv = scl_btree_node_vals(succ_node, ksz, maxk);
        scl_memcpy(nk + (size_t)idx * ksz, sk, ksz);
        scl_memcpy(nv + (size_t)idx * vsz, sv, vsz);
        return scl_btree_remove_recursive(alloc, tree, ch[idx + 1], sk, t, ksz,
                                          vsz, maxk);
      } else {
        /* Case 2c: both children have t-1 keys, merge then recurse */
        scl_btree_merge(alloc, node, idx, t, ksz, vsz, maxk);
        ch = scl_btree_node_children(node, ksz, vsz, maxk);
        return scl_btree_remove_recursive(alloc, tree, ch[idx], key, t, ksz,
                                          vsz, maxk);
      }
    }
  } else {
    /* Key not found in this node — descend to child */
    if (node->leaf)
      return false; /* Not found */

    /* Ensure the child we descend to has at least t keys */
    if (ch[idx]->count < (size_t)t) {
      /* Borrow from left sibling if possible */
      if (idx > 0 && ch[idx - 1]->count >= (size_t)t) {
        scl_btree_borrow_from_left(node, idx, t, ksz, vsz, maxk);
      }
      /* Borrow from right sibling if possible */
      else if (idx < (int)node->count && ch[idx + 1]->count >= (size_t)t) {
        scl_btree_borrow_from_right(node, idx, t, ksz, vsz, maxk);
      }
      /* Merge with sibling */
      else {
        int merge_idx = (idx > 0) ? (idx - 1) : idx;
        scl_btree_merge(alloc, node, merge_idx, t, ksz, vsz, maxk);
        /* After merge, child pointers have shifted; adjust idx */
        if (merge_idx == idx) {
          /* We merged child[idx] with child[idx+1], so child[idx] is the merged
           * node */
          idx = merge_idx;
        } else {
          /* We merged child[idx-1] with child[idx], so child[idx-1] is the
           * merged node */
          idx = merge_idx;
        }
      }
      /* Recompute child pointer after potential modifications */
      ch = scl_btree_node_children(node, ksz, vsz, maxk);
    }
    return scl_btree_remove_recursive(alloc, tree, ch[idx], key, t, ksz, vsz,
                                      maxk);
  }
}

scl_error_t scl_btree_remove(scl_allocator_t *alloc, scl_btree_t *tree,
                              const void *SCL_RESTRICT key) {
   if (scl_unlikely(!tree || !key))
     return SCL_ERR_NULL_PTR;
   if (scl_unlikely(!tree->root || tree->count == 0))
     return SCL_ERR_NOT_FOUND;

int t = tree->t;
    size_t ksz = tree->key_size;
    size_t vsz = tree->value_size;
    size_t maxk = (size_t)(2 * t - 1);

    bool removed = scl_btree_remove_recursive(alloc, tree, tree->root, key, t,
                                              ksz, vsz, maxk);

  /* If root becomes empty (and is not a leaf), shrink tree */
  if (tree->root->count == 0 && !tree->root->leaf) {
    scl_btree_node_t *old_root = tree->root;
    scl_btree_node_t **ch = scl_btree_node_children(old_root, ksz, vsz, maxk);
    tree->root = ch[0];
    scl_free(alloc, old_root);
  }

  if (removed) {
    tree->count--;
    return SCL_OK;
  }
  return SCL_ERR_NOT_FOUND;
}

size_t scl_btree_count(const scl_btree_t *tree) {
  return tree ? tree->count : 0;
}
bool scl_btree_empty(const scl_btree_t *tree) {
  return tree ? tree->count == 0 : true;
}
