#include "scl_rbtree.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static inline bool scl_rb_is_red(const scl_rbtree_node_t *n)
{
    return n && n->color == SCL_RB_RED;
}

scl_error_t scl_rbtree_init(scl_allocator_t *alloc, scl_rbtree_t *tree, size_t element_size, scl_cmp_func_t cmp)
{
    (void)alloc;
    if (!tree || !cmp) return SCL_ERR_NULL_PTR;
    if (element_size == 0) return SCL_ERR_INVALID_ARG;
    tree->root = NULL;
    tree->element_size = element_size;
    tree->count = 0;
    tree->cmp = cmp;
    return SCL_OK;
}

void scl_rbtree_destroy(scl_allocator_t *alloc, scl_rbtree_t *tree)
{
    if (!tree || !tree->root) return;
    size_t cap = 64;
    scl_rbtree_node_t **stack = scl_alloc(alloc, cap * sizeof(stack[0]), alignof(max_align_t));
    if (!stack) return;
    int sp = -1;
    scl_rbtree_node_t *cur = tree->root;
    scl_rbtree_node_t *last = NULL;
    while (cur || sp >= 0) {
        while (cur) {
            if ((size_t)(sp + 1) >= cap) {
                size_t new_cap = cap * 2;
                scl_rbtree_node_t **ns = scl_realloc(alloc, stack, cap * sizeof(stack[0]), new_cap * sizeof(stack[0]), alignof(max_align_t));
                if (!ns) { scl_free(alloc, stack); return; }
                stack = ns;
                cap = new_cap;
            }
            stack[++sp] = cur;
            cur = cur->left;
        }
        scl_rbtree_node_t *peek = stack[sp];
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

static scl_rbtree_node_t *scl_rbtree_create_node(scl_allocator_t *alloc, const void *data, size_t element_size)
{
    scl_rbtree_node_t *n = scl_alloc(alloc, sizeof(scl_rbtree_node_t), alignof(max_align_t));
    if (!n) return NULL;
    n->data = scl_alloc(alloc, element_size, alignof(max_align_t));
    if (!n->data) { scl_free(alloc, n); return NULL; }
    scl_memcpy(n->data, data, element_size);
    n->left = NULL;
    n->right = NULL;
    n->parent = NULL;
    n->color = SCL_RB_RED;
    return n;
}

static void scl_rbtree_left_rotate(scl_rbtree_t *tree, scl_rbtree_node_t *x)
{
    scl_rbtree_node_t *y = x->right;
    x->right = y->left;
    if (y->left) y->left->parent = x;
    y->parent = x->parent;
    if (!x->parent)
        tree->root = y;
    else if (x == x->parent->left)
        x->parent->left = y;
    else
        x->parent->right = y;
    y->left = x;
    x->parent = y;
}

static void scl_rbtree_right_rotate(scl_rbtree_t *tree, scl_rbtree_node_t *x)
{
    scl_rbtree_node_t *y = x->left;
    x->left = y->right;
    if (y->right) y->right->parent = x;
    y->parent = x->parent;
    if (!x->parent)
        tree->root = y;
    else if (x == x->parent->right)
        x->parent->right = y;
    else
        x->parent->left = y;
    y->right = x;
    x->parent = y;
}

static void scl_rbtree_insert_fixup(scl_rbtree_t *tree, scl_rbtree_node_t *z)
{
    while (scl_rb_is_red(z->parent)) {
        if (z->parent == z->parent->parent->left) {
            scl_rbtree_node_t *y = z->parent->parent->right;
            if (scl_rb_is_red(y)) {
                z->parent->color = SCL_RB_BLACK;
                y->color = SCL_RB_BLACK;
                z->parent->parent->color = SCL_RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    z = z->parent;
                    scl_rbtree_left_rotate(tree, z);
                }
                z->parent->color = SCL_RB_BLACK;
                z->parent->parent->color = SCL_RB_RED;
                scl_rbtree_right_rotate(tree, z->parent->parent);
            }
        } else {
            scl_rbtree_node_t *y = z->parent->parent->left;
            if (scl_rb_is_red(y)) {
                z->parent->color = SCL_RB_BLACK;
                y->color = SCL_RB_BLACK;
                z->parent->parent->color = SCL_RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    scl_rbtree_right_rotate(tree, z);
                }
                z->parent->color = SCL_RB_BLACK;
                z->parent->parent->color = SCL_RB_RED;
                scl_rbtree_left_rotate(tree, z->parent->parent);
            }
        }
    }
    tree->root->color = SCL_RB_BLACK;
}

scl_error_t scl_rbtree_insert(scl_allocator_t *alloc, scl_rbtree_t *tree, const void *element)
{
    if (!tree || !element) return SCL_ERR_NULL_PTR;

    scl_rbtree_node_t *z = scl_rbtree_create_node(alloc, element, tree->element_size);
    if (!z) return SCL_ERR_OUT_OF_MEMORY;

    scl_rbtree_node_t *y = NULL;
    scl_rbtree_node_t *x = tree->root;

    while (x) {
        y = x;
        int c = tree->cmp(z->data, x->data);
        if (c < 0)
            x = x->left;
        else if (c > 0)
            x = x->right;
        else {
            scl_memcpy(x->data, z->data, tree->element_size);
            scl_free(alloc, z->data);
            scl_free(alloc, z);
            return SCL_OK;
        }
    }

    z->parent = y;
    if (!y)
        tree->root = z;
    else if (tree->cmp(z->data, y->data) < 0)
        y->left = z;
    else
        y->right = z;

    scl_rbtree_insert_fixup(tree, z);
    tree->count++;
    return SCL_OK;
}

static inline bool scl_rb_is_black_or_null(const scl_rbtree_node_t *n) {
    return !n || n->color == SCL_RB_BLACK;
}

static void scl_rbtree_remove_fixup(scl_rbtree_t *tree,
                                     scl_rbtree_node_t *x,
                                     scl_rbtree_node_t *x_parent) {
    while (x != tree->root && scl_rb_is_black_or_null(x)) {
        bool x_is_left = (x == x_parent->left);
        scl_rbtree_node_t *w = x_is_left ? x_parent->right : x_parent->left;

        /* Case 1: sibling is red */
        if (w && w->color == SCL_RB_RED) {
            w->color = SCL_RB_BLACK;
            x_parent->color = SCL_RB_RED;
            if (x_is_left)
                scl_rbtree_left_rotate(tree, x_parent);
            else
                scl_rbtree_right_rotate(tree, x_parent);
            w = x_is_left ? x_parent->right : x_parent->left;
        }

        /* Case 2: sibling's children are both black (or sibling is NULL) */
        if (!w || (scl_rb_is_black_or_null(w->left) && scl_rb_is_black_or_null(w->right))) {
            if (w) w->color = SCL_RB_RED;
            x = x_parent;
            x_parent = x_parent->parent;
            continue;
        }

        /* Case 3: sibling's far child is black */
        if (x_is_left) {
            if (scl_rb_is_black_or_null(w->right)) {
                if (w->left) w->left->color = SCL_RB_BLACK;
                w->color = SCL_RB_RED;
                scl_rbtree_right_rotate(tree, w);
                w = x_parent->right;
            }
            /* Case 4: sibling's far child is red */
            w->color = x_parent->color;
            x_parent->color = SCL_RB_BLACK;
            if (w->right) w->right->color = SCL_RB_BLACK;
            scl_rbtree_left_rotate(tree, x_parent);
        } else {
            if (scl_rb_is_black_or_null(w->left)) {
                if (w->right) w->right->color = SCL_RB_BLACK;
                w->color = SCL_RB_RED;
                scl_rbtree_left_rotate(tree, w);
                w = x_parent->left;
            }
            w->color = x_parent->color;
            x_parent->color = SCL_RB_BLACK;
            if (w->left) w->left->color = SCL_RB_BLACK;
            scl_rbtree_right_rotate(tree, x_parent);
        }
        x = tree->root;
        x_parent = NULL;
    }
    if (x) x->color = SCL_RB_BLACK;
}

scl_error_t scl_rbtree_remove(scl_allocator_t *alloc, scl_rbtree_t *tree, const void *key)
{
    if (!tree || !key) return SCL_ERR_NULL_PTR;

    scl_rbtree_node_t *z = tree->root;
    while (z) {
        int c = tree->cmp(key, z->data);
        if (c < 0) z = z->left;
        else if (c > 0) z = z->right;
        else break;
    }
    if (!z) return SCL_ERR_NOT_FOUND;

    scl_rbtree_node_t *x, *y = z, *x_parent = NULL;
    scl_rb_color_t y_original_color = y->color;

    if (!z->left) {
        x = z->right;
        x_parent = z->parent;
        if (!z->parent)
            tree->root = x;
        else if (z == z->parent->left)
            z->parent->left = x;
        else
            z->parent->right = x;
        if (x) x->parent = z->parent;
    } else if (!z->right) {
        x = z->left;
        x_parent = z->parent;
        if (!z->parent)
            tree->root = x;
        else if (z == z->parent->left)
            z->parent->left = x;
        else
            z->parent->right = x;
        if (x) x->parent = z->parent;
    } else {
        y = z->right;
        while (y->left) y = y->left;
        y_original_color = y->color;
        x = y->right;

        if (y->parent == z) {
            x_parent = y;
            if (x) x->parent = y;
        } else {
            x_parent = y->parent;
            if (!y->parent)
                tree->root = x;
            else if (y == y->parent->left)
                y->parent->left = x;
            else
                y->parent->right = x;
            if (x) x->parent = y->parent;
            y->right = z->right;
            z->right->parent = y;
        }

        if (!z->parent)
            tree->root = y;
        else if (z == z->parent->left)
            z->parent->left = y;
        else
            z->parent->right = y;
        y->parent = z->parent;
        y->left = z->left;
        z->left->parent = y;
        y->color = z->color;
    }

    if (y_original_color == SCL_RB_BLACK)
        scl_rbtree_remove_fixup(tree, x, x_parent);

    scl_free(alloc, z->data);
    scl_free(alloc, z);
    tree->count--;
    return SCL_OK;
}

bool scl_rbtree_contains(const scl_rbtree_t *tree, const void *key)
{
    if (!tree || !key) return false;
    scl_rbtree_node_t *cur = tree->root;
    while (cur) {
        int c = tree->cmp(key, cur->data);
        if (c < 0) cur = cur->left;
        else if (c > 0) cur = cur->right;
        else return true;
    }
    return false;
}

scl_error_t scl_rbtree_find(const scl_rbtree_t *tree, const void *key, void *out)
{
    if (!tree || !key || !out) return SCL_ERR_NULL_PTR;
    scl_rbtree_node_t *cur = tree->root;
    while (cur) {
        int c = tree->cmp(key, cur->data);
        if (c < 0) cur = cur->left;
        else if (c > 0) cur = cur->right;
        else { scl_memcpy(out, cur->data, tree->element_size); return SCL_OK; }
    }
    return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_rbtree_min(const scl_rbtree_t *tree, void *out)
{
    if (!tree || !out) return SCL_ERR_NULL_PTR;
    if (!tree->root) return SCL_ERR_EMPTY;
    scl_rbtree_node_t *cur = tree->root;
    while (cur->left) cur = cur->left;
    scl_memcpy(out, cur->data, tree->element_size);
    return SCL_OK;
}

scl_error_t scl_rbtree_max(const scl_rbtree_t *tree, void *out)
{
    if (!tree || !out) return SCL_ERR_NULL_PTR;
    if (!tree->root) return SCL_ERR_EMPTY;
    scl_rbtree_node_t *cur = tree->root;
    while (cur->right) cur = cur->right;
    scl_memcpy(out, cur->data, tree->element_size);
    return SCL_OK;
}

size_t scl_rbtree_count(const scl_rbtree_t *tree) { return tree ? tree->count : 0; }
bool scl_rbtree_empty(const scl_rbtree_t *tree) { return tree ? tree->count == 0 : true; }

scl_error_t scl_rbtree_inorder(const scl_rbtree_t *tree, scl_visit_func_t visit, void *ctx)
{
    if (!tree || !visit) return SCL_ERR_NULL_PTR;
    scl_rbtree_node_t *stack[256];
    int sp = -1;
    scl_rbtree_node_t *cur = tree->root;
    while (cur || sp >= 0) {
        while (cur) {
            if (sp >= 255) return SCL_ERR_INVALID_STATE;
            stack[++sp] = cur;
            cur = cur->left;
        }
        cur = stack[sp--];
        visit(cur->data, ctx);
        cur = cur->right;
    }
    return SCL_OK;
}
