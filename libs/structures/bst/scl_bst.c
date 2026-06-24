#include "scl_bst.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_bst_init(scl_allocator_t *alloc, scl_bst_t *tree, size_t element_size, scl_cmp_func_t cmp)
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

void scl_bst_destroy(scl_allocator_t *alloc, scl_bst_t *tree)
{
    if (scl_unlikely(!tree || !tree->root)) return;
    size_t cap = 64;
    scl_bst_node_t **stack = scl_alloc(alloc, cap * sizeof(stack[0]), alignof(max_align_t));
    if (scl_unlikely(!stack)) return;
    int sp = -1;
    scl_bst_node_t *cur = tree->root;
    scl_bst_node_t *last = NULL;
    while (cur || sp >= 0) {
        while (scl_likely(cur)) {
            if ((size_t)(sp + 1) >= cap) {
                size_t new_cap = cap * 2;
                scl_bst_node_t **ns = scl_realloc(alloc, stack, cap * sizeof(stack[0]), new_cap * sizeof(stack[0]), alignof(max_align_t));
                if (!ns) { scl_free(alloc, stack); return; }
                stack = ns;
                cap = new_cap;
            }
            stack[++sp] = cur;
            cur = cur->left;
        }
        scl_bst_node_t *peek = stack[sp];
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

static scl_error_t scl_bst_create_node(scl_allocator_t *alloc, scl_bst_node_t **out, const void *data, size_t element_size)
{
    scl_bst_node_t *node = scl_alloc(alloc, sizeof(scl_bst_node_t), alignof(max_align_t));
    if (scl_unlikely(!node)) return SCL_ERR_OUT_OF_MEMORY;

    node->data = scl_alloc(alloc, element_size, alignof(max_align_t));
    if (scl_unlikely(!node->data)) {
        scl_free(alloc, node);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    scl_memcpy(node->data, data, element_size);
    node->left = NULL;
    node->right = NULL;
    *out = node;
    return SCL_OK;
}

scl_error_t scl_bst_insert(scl_allocator_t *alloc, scl_bst_t *tree, const void  *SCL_RESTRICT element)
{
    if (scl_unlikely(!tree || !element)) return SCL_ERR_NULL_PTR;

    scl_bst_node_t *node;
    scl_error_t err = scl_bst_create_node(alloc, &node, element, tree->element_size);
    if (err != SCL_OK) return err;

    if (!tree->root) {
        tree->root = node;
        tree->count = 1;
        return SCL_OK;
    }

    scl_bst_node_t *cur = tree->root;
    while (1) {
        int c = tree->cmp(element, cur->data);
        if (c < 0) {
            if (!cur->left) { cur->left = node; break; }
            cur = cur->left;
        } else if (c > 0) {
            if (!cur->right) { cur->right = node; break; }
            cur = cur->right;
        } else {
            scl_memcpy(cur->data, element, tree->element_size);
            scl_free(alloc, node->data);
            scl_free(alloc, node);
            return SCL_OK;
        }
    }
    tree->count++;
    return SCL_OK;
}

scl_error_t scl_bst_remove(scl_allocator_t *alloc, scl_bst_t *tree, const void  *SCL_RESTRICT key)
{
    if (scl_unlikely(!tree || !key)) return SCL_ERR_NULL_PTR;

    scl_bst_node_t *parent = NULL;
    scl_bst_node_t *cur = tree->root;

    while (scl_likely(cur)) {
        int c = tree->cmp(key, cur->data);
        if (c < 0) { parent = cur; cur = cur->left; }
        else if (c > 0) { parent = cur; cur = cur->right; }
        else break;
    }
    if (scl_unlikely(!cur)) return SCL_ERR_NOT_FOUND;

    if (cur->left && cur->right) {
        scl_bst_node_t *succ_parent = cur;
        scl_bst_node_t *succ = cur->right;
        while (scl_likely(succ->left)) {
            succ_parent = succ;
            succ = succ->left;
        }
        scl_memcpy(cur->data, succ->data, tree->element_size);
        cur = succ;
        parent = succ_parent;
    }

    scl_bst_node_t *child = cur->left ? cur->left : cur->right;
    if (!parent) {
        tree->root = child;
    } else if (parent->left == cur) {
        parent->left = child;
    } else {
        parent->right = child;
    }

    scl_free(alloc, cur->data);
    scl_free(alloc, cur);
    tree->count--;
    return SCL_OK;
}

bool scl_bst_contains(const scl_bst_t *tree, const void *key)
{
    if (scl_unlikely(!tree || !key)) return false;
    scl_bst_node_t *cur = tree->root;
    while (scl_likely(cur)) {
        int c = tree->cmp(key, cur->data);
        if (c < 0) cur = cur->left;
        else if (c > 0) cur = cur->right;
        else return true;
    }
    return false;
}

scl_error_t scl_bst_find(const scl_bst_t *tree, const void *key, void  *SCL_RESTRICT out)
{
    if (scl_unlikely(!tree || !key || !out)) return SCL_ERR_NULL_PTR;
    scl_bst_node_t *cur = tree->root;
    while (scl_likely(cur)) {
        int c = tree->cmp(key, cur->data);
        if (c < 0) cur = cur->left;
        else if (c > 0) cur = cur->right;
        else { scl_memcpy(out, cur->data, tree->element_size); return SCL_OK; }
    }
    return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_bst_min(const scl_bst_t *tree, void *out)
{
    if (scl_unlikely(!tree || !out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!tree->root)) return SCL_ERR_EMPTY;
    scl_bst_node_t *cur = tree->root;
    while (cur->left) cur = cur->left;
    scl_memcpy(out, cur->data, tree->element_size);
    return SCL_OK;
}

scl_error_t scl_bst_max(const scl_bst_t *tree, void *out)
{
    if (scl_unlikely(!tree || !out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!tree->root)) return SCL_ERR_EMPTY;
    scl_bst_node_t *cur = tree->root;
    while (cur->right) cur = cur->right;
    scl_memcpy(out, cur->data, tree->element_size);
    return SCL_OK;
}

size_t scl_bst_count(const scl_bst_t *tree)
{
    return tree ? tree->count : 0;
}

bool scl_bst_empty(const scl_bst_t *tree)
{
    return tree ? tree->count == 0 : true;
}

scl_error_t scl_bst_inorder(const scl_bst_t *tree, scl_visit_func_t visit, void  *SCL_RESTRICT ctx)
{
    if (scl_unlikely(!tree || !visit)) return SCL_ERR_NULL_PTR;

    scl_bst_node_t *stack[256];
    int sp = -1;
    scl_bst_node_t *cur = tree->root;

    while (cur || sp >= 0) {
        while (scl_likely(cur)) {
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
