#include "scl_skiplist.h"
#include "scl_string.h"
#include <time.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static size_t scl_skiplist_random_level(void)
{
    size_t level = 1;
    while ((rand() % 2) && level < SCL_SKIPLIST_MAX_LEVEL)
        level++;
    return level;
}

scl_error_t scl_skiplist_init(scl_allocator_t *alloc, scl_skiplist_t *sl, size_t element_size, scl_cmp_func_t cmp)
{
    if (!sl || !cmp) return SCL_ERR_NULL_PTR;
    if (element_size == 0) return SCL_ERR_INVALID_ARG;

    sl->head = scl_alloc(alloc, sizeof(scl_skiplist_node_t), alignof(max_align_t));
    if (!sl->head) return SCL_ERR_OUT_OF_MEMORY;

    sl->head->data = NULL;
    sl->head->forward = scl_calloc(alloc, SCL_SKIPLIST_MAX_LEVEL, sizeof(scl_skiplist_node_t *), alignof(max_align_t));
    if (!sl->head->forward) {
        scl_free(alloc, sl->head);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    sl->head->level = SCL_SKIPLIST_MAX_LEVEL;

    sl->element_size = element_size;
    sl->count = 0;
    sl->cmp = cmp;
    sl->level = 1;
    return SCL_OK;
}

void scl_skiplist_destroy(scl_allocator_t *alloc, scl_skiplist_t *sl)
{
    if (!sl) return;
    scl_skiplist_node_t *cur = sl->head;
    while (cur) {
        scl_skiplist_node_t *next = cur->forward[0];
        scl_free(alloc, cur->data);
        scl_free(alloc, cur->forward);
        scl_free(alloc, cur);
        cur = next;
    }
    sl->head = NULL;
    sl->count = 0;
}

scl_error_t scl_skiplist_insert(scl_allocator_t *alloc, scl_skiplist_t *sl, const void *element)
{
    if (!sl || !element) return SCL_ERR_NULL_PTR;

    scl_skiplist_node_t *update[SCL_SKIPLIST_MAX_LEVEL];
    scl_skiplist_node_t *cur = sl->head;

    for (int i = (int)sl->level - 1; i >= 0; i--) {
        while (cur->forward[i] && sl->cmp(cur->forward[i]->data, element) < 0)
            cur = cur->forward[i];
        update[i] = cur;
    }

    cur = cur->forward[0];
    if (cur && sl->cmp(cur->data, element) == 0) {
        scl_memcpy(cur->data, element, sl->element_size);
        return SCL_OK;
    }

    size_t new_level = scl_skiplist_random_level();
    if (new_level > sl->level) {
        for (size_t i = sl->level; i < new_level; i++)
            update[i] = sl->head;
        sl->level = new_level;
    }

    scl_skiplist_node_t *node = scl_alloc(alloc, sizeof(scl_skiplist_node_t), alignof(max_align_t));
    if (!node) return SCL_ERR_OUT_OF_MEMORY;
    node->data = scl_alloc(alloc, sl->element_size, alignof(max_align_t));
    if (!node->data) { scl_free(alloc, node); return SCL_ERR_OUT_OF_MEMORY; }
    scl_memcpy(node->data, element, sl->element_size);
    node->forward = scl_calloc(alloc, new_level, sizeof(scl_skiplist_node_t *), alignof(max_align_t));
    if (!node->forward) { scl_free(alloc, node->data); scl_free(alloc, node); return SCL_ERR_OUT_OF_MEMORY; }
    node->level = new_level;

    for (size_t i = 0; i < new_level; i++) {
        node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = node;
    }
    sl->count++;
    return SCL_OK;
}

scl_error_t scl_skiplist_remove(scl_allocator_t *alloc, scl_skiplist_t *sl, const void *key)
{
    if (!sl || !key) return SCL_ERR_NULL_PTR;

    scl_skiplist_node_t *update[SCL_SKIPLIST_MAX_LEVEL];
    scl_skiplist_node_t *cur = sl->head;

    for (int i = (int)sl->level - 1; i >= 0; i--) {
        while (cur->forward[i] && sl->cmp(cur->forward[i]->data, key) < 0)
            cur = cur->forward[i];
        update[i] = cur;
    }

    cur = cur->forward[0];
    if (!cur || sl->cmp(cur->data, key) != 0)
        return SCL_ERR_NOT_FOUND;

    for (size_t i = 0; i < sl->level; i++) {
        if (update[i]->forward[i] != cur) break;
        update[i]->forward[i] = cur->forward[i];
    }

    scl_free(alloc, cur->data);
    scl_free(alloc, cur->forward);
    scl_free(alloc, cur);
    sl->count--;

    while (sl->level > 1 && !sl->head->forward[sl->level - 1])
        sl->level--;
    return SCL_OK;
}

bool scl_skiplist_contains(const scl_skiplist_t *sl, const void *key)
{
    if (!sl || !key) return false;
    scl_skiplist_node_t *cur = sl->head;
    for (int i = (int)sl->level - 1; i >= 0; i--) {
        while (cur->forward[i] && sl->cmp(cur->forward[i]->data, key) < 0)
            cur = cur->forward[i];
    }
    cur = cur->forward[0];
    return cur && sl->cmp(cur->data, key) == 0;
}

scl_error_t scl_skiplist_find(const scl_skiplist_t *sl, const void *key, void *out)
{
    if (!sl || !key || !out) return SCL_ERR_NULL_PTR;
    scl_skiplist_node_t *cur = sl->head;
    for (int i = (int)sl->level - 1; i >= 0; i--) {
        while (cur->forward[i] && sl->cmp(cur->forward[i]->data, key) < 0)
            cur = cur->forward[i];
    }
    cur = cur->forward[0];
    if (!cur || sl->cmp(cur->data, key) != 0) return SCL_ERR_NOT_FOUND;
    scl_memcpy(out, cur->data, sl->element_size);
    return SCL_OK;
}

size_t scl_skiplist_count(const scl_skiplist_t *sl) { return sl ? sl->count : 0; }
bool scl_skiplist_empty(const scl_skiplist_t *sl) { return sl ? sl->count == 0 : true; }
