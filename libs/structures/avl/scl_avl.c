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

/* avl data structure. */

#include "scl_avl.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static SCL_ALWAYS_INLINE int scl_avl_height(const scl_avl_node_t *n)
{
    return n ? n->height : -1;
}

static SCL_ALWAYS_INLINE int scl_avl_max2(int a, int b)
{
    return a > b ? a : b;
}

static SCL_ALWAYS_INLINE void scl_avl_update_height(scl_avl_node_t *n)
{
    n->height = 1 + scl_avl_max2(scl_avl_height(n->left), scl_avl_height(n->right));
}

static SCL_ALWAYS_INLINE int scl_avl_balance(const scl_avl_node_t *n)
{
    return n ? scl_avl_height(n->left) - scl_avl_height(n->right) : 0;
}

static scl_avl_node_t *scl_avl_rotate_right(scl_avl_node_t *y)
{
    scl_avl_node_t *x = y->left;
    scl_avl_node_t *t2 = x->right;
    x->right = y;
    y->left = t2;
    scl_avl_update_height(y);
    scl_avl_update_height(x);
    return x;
}

static scl_avl_node_t *scl_avl_rotate_left(scl_avl_node_t *x)
{
    scl_avl_node_t *y = x->right;
    scl_avl_node_t *t2 = y->left;
    y->left = x;
    x->right = t2;
    scl_avl_update_height(x);
    scl_avl_update_height(y);
    return y;
}

scl_error_t scl_avl_init(scl_allocator_t *alloc, scl_avl_t *tree, size_t element_size, scl_cmp_func_t cmp)
{
    (void)alloc;
    if (scl_unlikely(!tree || !cmp)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(element_size == 0)) return SCL_ERR_INVALID_ARG;
    tree->root = NULL;
    tree->element_size = element_size;
    tree->count = 0;
    tree->cmp = cmp;
    return SCL_OK;
}

void scl_avl_destroy(scl_allocator_t *alloc, scl_avl_t *tree)
{
    if (scl_unlikely(!tree || !tree->root)) return;
    size_t cap = 64;
    scl_avl_node_t **stack = scl_alloc(alloc, cap * sizeof(stack[0]), alignof(max_align_t));
    if (scl_unlikely(!stack)) return;
    int sp = -1;
    scl_avl_node_t *cur = tree->root;
    scl_avl_node_t *last = NULL;
    while (cur || scl_likely(sp >= 0)) {
        while (cur) {
            if (scl_unlikely((size_t)(sp + 1) >= cap)) {
                size_t new_cap = cap * 2;
                scl_avl_node_t **ns = scl_realloc(alloc, stack, cap * sizeof(stack[0]), new_cap * sizeof(stack[0]), alignof(max_align_t));
                if (scl_unlikely(!ns)) { scl_free(alloc, stack); return; }
                stack = ns;
                cap = new_cap;
            }
            stack[++sp] = cur;
            cur = cur->left;
        }
        scl_avl_node_t *peek = stack[sp];
        if (peek->right && last != peek->right) {
            cur = peek->right;
        } else {
            sp--;
            scl_free(alloc, peek->data);
            scl_free(alloc, peek);
            last = peek;
        }
    }
    scl_free(alloc, stack);
    tree->root = NULL;
    tree->count = 0;
}

static scl_avl_node_t *scl_avl_create_node(scl_allocator_t *alloc, const void *data, size_t element_size)
{
    scl_avl_node_t *n = scl_alloc(alloc, sizeof(scl_avl_node_t), alignof(max_align_t));
    if (scl_unlikely(!n)) return NULL;
    n->data = scl_alloc(alloc, element_size, alignof(max_align_t));
    if (scl_unlikely(!n->data)) { scl_free(alloc, n); return NULL; }
    scl_memcpy(n->data, data, element_size);
    n->left = NULL;
    n->right = NULL;
    n->height = 0;
    return n;
}

static __attribute__((unused)) scl_avl_node_t *scl_avl_balance_node(scl_avl_node_t *n, const void *data, scl_cmp_func_t cmp)
{
    (void)data;
    (void)cmp;
    scl_avl_update_height(n);
    int bal = scl_avl_balance(n);

    if (bal > 1) {
        if (scl_avl_balance(n->left) >= 0)
            return scl_avl_rotate_right(n);
        n->left = scl_avl_rotate_left(n->left);
        return scl_avl_rotate_right(n);
    }
    if (bal < -1) {
        if (scl_avl_balance(n->right) <= 0)
            return scl_avl_rotate_left(n);
        n->right = scl_avl_rotate_right(n->right);
        return scl_avl_rotate_left(n);
    }
    return n;
}

scl_error_t scl_avl_insert(scl_allocator_t *alloc, scl_avl_t *tree, const void  *SCL_RESTRICT element)
{
    if (scl_unlikely(!tree || !element)) return SCL_ERR_NULL_PTR;

    scl_avl_node_t *node = scl_avl_create_node(alloc, element, tree->element_size);
    if (scl_unlikely(!node)) return SCL_ERR_OUT_OF_MEMORY;

    if (!tree->root) {
        tree->root = node;
        tree->count = 1;
        return SCL_OK;
    }

    scl_avl_node_t *path[256];
    int path_len = 0;
    scl_avl_node_t *cur = tree->root;

    while (1) {
        path[path_len++] = cur;
        int c = tree->cmp(element, cur->data);
        if (c < 0) {
            if (!cur->left) {
                cur->left = node;
                break;
            }
            cur = cur->left;
        } else if (c > 0) {
            if (!cur->right) {
                cur->right = node;
                break;
            }
            cur = cur->right;
        } else {
            scl_memcpy(cur->data, element, tree->element_size);
            scl_free(alloc, node->data);
            scl_free(alloc, node);
            return SCL_OK;
        }
    }

    for (int i = path_len - 1; i >= 0; i--) {
        scl_avl_node_t *n = path[i];
        int bal = scl_avl_balance(n);
        scl_avl_node_t *new_sub;

        if (bal > 1) {
            if (scl_avl_balance(n->left) >= 0) {
                new_sub = scl_avl_rotate_right(n);
            } else {
                n->left = scl_avl_rotate_left(n->left);
                new_sub = scl_avl_rotate_right(n);
            }
        } else if (bal < -1) {
            if (scl_avl_balance(n->right) <= 0) {
                new_sub = scl_avl_rotate_left(n);
            } else {
                n->right = scl_avl_rotate_right(n->right);
                new_sub = scl_avl_rotate_left(n);
            }
        } else {
            scl_avl_update_height(n);
            continue;
        }

        if (i == 0) {
            tree->root = new_sub;
        } else {
            scl_avl_node_t *p = path[i - 1];
            if (p->left == n) p->left = new_sub;
            else p->right = new_sub;
        }
    }

    tree->count++;
    return SCL_OK;
}

static __attribute__((unused)) SCL_ALWAYS_INLINE scl_avl_node_t *scl_avl_min_node(scl_avl_node_t *n)
{
    while (n && n->left) n = n->left;
    return n;
}

scl_error_t scl_avl_remove(scl_allocator_t *alloc, scl_avl_t *tree, const void  *SCL_RESTRICT key)
{
    if (scl_unlikely(!tree || !key)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!tree->root)) return SCL_ERR_NOT_FOUND;

    scl_avl_node_t *path[256];
    int path_len = 0;
    scl_avl_node_t *cur = tree->root;
    bool found = false;

    while (scl_likely(cur)) {
        path[path_len++] = cur;
        int c = tree->cmp(key, cur->data);
        if (c < 0) {
            cur = cur->left;
        } else if (c > 0) {
            cur = cur->right;
        } else {
            found = true;
            break;
        }
    }
    if (scl_unlikely(!found)) return SCL_ERR_NOT_FOUND;

    scl_avl_node_t *node = path[--path_len];

    if (!node->left || !node->right) {
        scl_avl_node_t *child = node->left ? node->left : node->right;
        if (path_len == 0) {
            tree->root = child;
        } else {
            scl_avl_node_t *p = path[path_len - 1];
            if (p->left == node) p->left = child;
            else p->right = child;
        }
        scl_free(alloc, node->data);
        scl_free(alloc, node);
    } else {
        scl_avl_node_t *succ = node->right;
        int succ_idx = path_len;
        while (scl_likely(succ->left)) {
            if (scl_unlikely(succ_idx >= 255)) return SCL_ERR_INVALID_STATE;
            if (succ_idx == path_len) path[succ_idx] = node;
            path[succ_idx++] = succ;
            succ = succ->left;
        }
        if (succ_idx == path_len) path[succ_idx] = node;
        path[succ_idx++] = succ;

        scl_memcpy(node->data, succ->data, tree->element_size);

        scl_avl_node_t *succ_parent = path[succ_idx - 1];
        scl_avl_node_t *succ_child = succ->right;
        if (succ_parent->left == succ)
            succ_parent->left = succ_child;
        else
            succ_parent->right = succ_child;
        scl_free(alloc, succ->data);
        scl_free(alloc, succ);

        path_len = succ_idx - 1;
    }

    for (int i = path_len - 1; i >= 0; i--) {
        scl_avl_node_t *n = path[i];
        scl_avl_update_height(n);
        int bal = scl_avl_balance(n);
        scl_avl_node_t *new_sub;

        if (bal > 1) {
            if (scl_avl_balance(n->left) >= 0)
                new_sub = scl_avl_rotate_right(n);
            else {
                n->left = scl_avl_rotate_left(n->left);
                new_sub = scl_avl_rotate_right(n);
            }
        } else if (bal < -1) {
            if (scl_avl_balance(n->right) <= 0)
                new_sub = scl_avl_rotate_left(n);
            else {
                n->right = scl_avl_rotate_right(n->right);
                new_sub = scl_avl_rotate_left(n);
            }
        } else {
            continue;
        }

        if (i == 0)
            tree->root = new_sub;
        else {
            scl_avl_node_t *p = path[i - 1];
            if (p->left == n) p->left = new_sub;
            else p->right = new_sub;
        }
    }

    tree->count--;
    return SCL_OK;
}

bool scl_avl_contains(const scl_avl_t *tree, const void *key)
{
    if (scl_unlikely(!tree || !key)) return false;
    scl_avl_node_t *cur = tree->root;
    while (scl_likely(cur)) {
        int c = tree->cmp(key, cur->data);
        if (c < 0) cur = cur->left;
        else if (c > 0) cur = cur->right;
        else return true;
    }
    return false;
}

scl_error_t scl_avl_find(const scl_avl_t *tree, const void *key, void  *SCL_RESTRICT out)
{
    if (scl_unlikely(!tree || !key || !out)) return SCL_ERR_NULL_PTR;
    scl_avl_node_t *cur = tree->root;
    while (scl_likely(cur)) {
        int c = tree->cmp(key, cur->data);
        if (c < 0) cur = cur->left;
        else if (c > 0) cur = cur->right;
        else { scl_memcpy(out, cur->data, tree->element_size); return SCL_OK; }
    }
    return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_avl_min(const scl_avl_t *tree, void *out)
{
    if (scl_unlikely(!tree || !out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!tree->root)) return SCL_ERR_EMPTY;
    scl_avl_node_t *cur = tree->root;
    while (scl_likely(cur->left)) cur = cur->left;
    scl_memcpy(out, cur->data, tree->element_size);
    return SCL_OK;
}

scl_error_t scl_avl_max(const scl_avl_t *tree, void *out)
{
    if (scl_unlikely(!tree || !out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!tree->root)) return SCL_ERR_EMPTY;
    scl_avl_node_t *cur = tree->root;
    while (scl_likely(cur->right)) cur = cur->right;
    scl_memcpy(out, cur->data, tree->element_size);
    return SCL_OK;
}

size_t scl_avl_count(const scl_avl_t *tree) { return scl_likely(tree) ? tree->count : 0; }
bool scl_avl_empty(const scl_avl_t *tree) { return scl_likely(tree) ? tree->count == 0 : true; }

scl_error_t scl_avl_inorder(const scl_avl_t *tree, scl_visit_func_t visit, void  *SCL_RESTRICT ctx)
{
    if (scl_unlikely(!tree || !visit)) return SCL_ERR_NULL_PTR;
    scl_avl_node_t *stack[256];
    int sp = -1;
    scl_avl_node_t *cur = tree->root;
    while (cur || scl_likely(sp >= 0)) {
        while (cur) {
            if (scl_unlikely(sp >= 255)) return SCL_ERR_INVALID_STATE;
            stack[++sp] = cur;
            cur = cur->left;
        }
        cur = stack[sp--];
        visit(cur->data, ctx);
        cur = cur->right;
    }
    return SCL_OK;
}
