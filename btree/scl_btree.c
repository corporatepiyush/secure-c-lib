#include "scl_btree.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static scl_btree_node_t *scl_btree_create_node(scl_allocator_t *alloc, bool leaf, int t,
                                                size_t ksz, size_t vsz)
{
    size_t maxk = (size_t)(2 * t - 1);
    size_t maxc = (size_t)(2 * t);
    size_t sz = sizeof(scl_btree_node_t) + ksz * maxk + vsz * maxk + sizeof(scl_btree_node_t *) * maxc;
    scl_btree_node_t *node = scl_alloc(alloc, sz, alignof(max_align_t));
    if (!node) return NULL;
    memset(node, 0, sz);
    node->count = 0;
    node->leaf = leaf;
    return node;
}

void scl_btree_destroy(scl_allocator_t *alloc, scl_btree_t *tree)
{
    if (!tree || !tree->root) return;

    scl_btree_node_t *stack[256];
    int sp = 0;
    stack[sp++] = tree->root;

    while (sp > 0) {
        scl_btree_node_t *node = stack[--sp];
        if (!node->leaf) {
            size_t maxk = (size_t)(2 * tree->t - 1);
            scl_btree_node_t **ch = scl_btree_node_children(node, tree->key_size, tree->value_size, maxk);
            for (size_t i = 0; i <= node->count; i++)
                if (ch[i])
                    stack[sp++] = ch[i];
        }
        scl_free(alloc, node);
    }

    tree->root = NULL;
    tree->count = 0;
}

scl_error_t scl_btree_init(scl_allocator_t *alloc, scl_btree_t *tree, size_t key_size, size_t value_size,
                           int degree, scl_cmp_func_t cmp)
{
    if (!tree || !cmp) return SCL_ERR_NULL_PTR;
    if (key_size == 0 || value_size == 0 || degree < 2)
        return SCL_ERR_INVALID_ARG;

    tree->root = scl_btree_create_node(alloc, true, degree, key_size, value_size);
    if (!tree->root) return SCL_ERR_OUT_OF_MEMORY;

    tree->key_size = key_size;
    tree->value_size = value_size;
    tree->count = 0;
    tree->cmp = cmp;
    tree->t = degree;
    return SCL_OK;
}

static void scl_btree_split_child(scl_allocator_t *alloc, scl_btree_node_t *parent, size_t i, int t,
                                   size_t ksz, size_t vsz)
{
    size_t maxk = (size_t)(2 * t - 1);
    scl_btree_node_t **pch = scl_btree_node_children(parent, ksz, vsz, maxk);
    scl_btree_node_t *child = pch[i];
    scl_btree_node_t *new_child = scl_btree_create_node(alloc, child->leaf, t, ksz, vsz);
    if (!new_child) return;

    unsigned char *ck = scl_btree_node_keys(child);
    unsigned char *cv = scl_btree_node_vals(child, ksz, maxk);
    unsigned char *nk = scl_btree_node_keys(new_child);
    unsigned char *nv = scl_btree_node_vals(new_child, ksz, maxk);
    scl_btree_node_t **cch = scl_btree_node_children(child, ksz, vsz, maxk);
    scl_btree_node_t **nch = scl_btree_node_children(new_child, ksz, vsz, maxk);

    new_child->count = (size_t)(t - 1);
    for (int j = 0; j < t - 1; j++) {
        memcpy(nk + (size_t)j * ksz, ck + (size_t)(j + t) * ksz, ksz);
        memcpy(nv + (size_t)j * vsz, cv + (size_t)(j + t) * vsz, vsz);
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
    for (size_t j = parent->count; j > i; j--) {
        memcpy(pk + j * ksz, pk + (j - 1) * ksz, ksz);
        memcpy(pv + j * vsz, pv + (j - 1) * vsz, vsz);
    }
    memcpy(pk + i * ksz, ck + (size_t)(t - 1) * ksz, ksz);
    memcpy(pv + i * vsz, cv + (size_t)(t - 1) * vsz, vsz);
    parent->count++;
}

scl_error_t scl_btree_insert(scl_allocator_t *alloc, scl_btree_t *tree, const void *key, const void *value)
{
    if (!tree || !key || !value) return SCL_ERR_NULL_PTR;

    int t = tree->t;
    size_t ksz = tree->key_size;
    size_t vsz = tree->value_size;
    size_t maxk = (size_t)(2 * t - 1);
    scl_cmp_func_t cmp = tree->cmp;

    if (tree->root->count == maxk) {
        scl_btree_node_t *new_root = scl_btree_create_node(alloc, false, t, ksz, vsz);
        if (!new_root) return SCL_ERR_OUT_OF_MEMORY;
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
            int i = (int)node->count - 1;
            while (i >= 0 && cmp(key, nk + (size_t)i * ksz) < 0) {
                memcpy(nk + (size_t)(i + 1) * ksz, nk + (size_t)i * ksz, ksz);
                memcpy(nv + (size_t)(i + 1) * vsz, nv + (size_t)i * vsz, vsz);
                i--;
            }
            i++;
            if ((size_t)i < node->count && cmp(key, nk + (size_t)i * ksz) == 0) {
                memcpy(nv + (size_t)i * vsz, value, vsz);
                return SCL_OK;
            }
            memcpy(nk + (size_t)i * ksz, key, ksz);
            memcpy(nv + (size_t)i * vsz, value, vsz);
            node->count++;
            break;
        } else {
            int i = (int)node->count - 1;
            while (i >= 0 && cmp(key, nk + (size_t)i * ksz) < 0)
                i--;
            i++;
            if ((size_t)i < node->count && cmp(key, nk + (size_t)i * ksz) == 0) {
                memcpy(nv + (size_t)i * vsz, value, vsz);
                return SCL_OK;
            }
            scl_btree_node_t **ch = scl_btree_node_children(node, ksz, vsz, maxk);
            if (ch[i]->count == maxk) {
                scl_btree_split_child(alloc, node, (size_t)i, t, ksz, vsz);
                if (cmp(key, scl_btree_node_keys(node) + (size_t)i * ksz) > 0)
                    i++;
            }
            node = scl_btree_node_children(node, ksz, vsz, maxk)[(size_t)i];
        }
    }

    tree->count++;
    return SCL_OK;
}

scl_error_t scl_btree_get(const scl_btree_t *tree, const void *key, void *out_value)
{
    if (!tree || !key || !out_value) return SCL_ERR_NULL_PTR;

    int t = tree->t;
    size_t ksz = tree->key_size;
    size_t vsz = tree->value_size;
    size_t maxk = (size_t)(2 * t - 1);
    scl_cmp_func_t cmp = tree->cmp;

    scl_btree_node_t *node = tree->root;
    while (node) {
        unsigned char *nk = scl_btree_node_keys(node);
        unsigned char *nv = scl_btree_node_vals(node, ksz, maxk);

        size_t lo = 0, hi = node->count;
        while (lo < hi) {
            size_t mid = lo + (hi - lo) / 2;
            int r = cmp(key, nk + mid * ksz);
            if (r == 0) {
                memcpy(out_value, nv + mid * vsz, vsz);
                return SCL_OK;
            }
            if (r < 0) hi = mid;
            else lo = mid + 1;
        }
        if (node->leaf) break;
        scl_btree_node_t **ch = scl_btree_node_children(node, ksz, vsz, maxk);
        node = ch[lo];
    }
    return SCL_ERR_NOT_FOUND;
}

bool scl_btree_contains(const scl_btree_t *tree, const void *key)
{
    if (!tree || !key) return false;

    int t = tree->t;
    size_t ksz = tree->key_size;
    size_t maxk = (size_t)(2 * t - 1);
    scl_cmp_func_t cmp = tree->cmp;

    scl_btree_node_t *node = tree->root;
    while (node) {
        unsigned char *nk = scl_btree_node_keys(node);

        size_t lo = 0, hi = node->count;
        while (lo < hi) {
            size_t mid = lo + (hi - lo) / 2;
            int r = cmp(key, nk + mid * ksz);
            if (r == 0) return true;
            if (r < 0) hi = mid;
            else lo = mid + 1;
        }
        if (node->leaf) break;
        scl_btree_node_t **ch = scl_btree_node_children(node, ksz, 0, maxk);
        node = ch[lo];
    }
    return false;
}

scl_error_t scl_btree_remove(scl_allocator_t *alloc, scl_btree_t *tree, const void *key)
{
    (void)alloc;
    (void)tree;
    (void)key;
    return SCL_ERR_NOT_IMPLEMENTED;
}

size_t scl_btree_count(const scl_btree_t *tree) { return tree ? tree->count : 0; }
bool scl_btree_empty(const scl_btree_t *tree) { return tree ? tree->count == 0 : true; }
