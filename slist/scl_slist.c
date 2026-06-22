#include "scl_slist.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_slist_init(scl_slist_t *list, size_t element_size)
{
    if (!list) return SCL_ERR_NULL_PTR;
    if (element_size == 0) return SCL_ERR_INVALID_ARG;

    list->head = NULL;
    list->tail = NULL;
    list->element_size = element_size;
    list->count = 0;
    return SCL_OK;
}

void scl_slist_destroy(scl_allocator_t *alloc, scl_slist_t *list)
{
    if (!list) return;
    scl_slist_node_t *cur = list->head;
    while (cur) {
        scl_slist_node_t *next = cur->next;
        scl_free(alloc, cur->data);
        scl_free(alloc, cur);
        cur = next;
    }
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}

static scl_error_t scl_slist_create_node(scl_allocator_t *alloc, scl_slist_node_t **out, const void *data, size_t element_size)
{
    scl_slist_node_t *node = scl_alloc(alloc, sizeof(scl_slist_node_t), alignof(max_align_t));
    if (!node) return SCL_ERR_OUT_OF_MEMORY;

    node->data = scl_alloc(alloc, element_size, alignof(max_align_t));
    if (!node->data) {
        scl_free(alloc, node);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    memcpy(node->data, data, element_size);
    node->next = NULL;
    *out = node;
    return SCL_OK;
}

scl_error_t scl_slist_push_front(scl_allocator_t *alloc, scl_slist_t *list, const void *element)
{
    if (!list || !element) return SCL_ERR_NULL_PTR;

    scl_slist_node_t *node;
    scl_error_t err = scl_slist_create_node(alloc, &node, element, list->element_size);
    if (err != SCL_OK) return err;

    node->next = list->head;
    list->head = node;
    if (!list->tail) list->tail = node;
    list->count++;
    return SCL_OK;
}

scl_error_t scl_slist_push_back(scl_allocator_t *alloc, scl_slist_t *list, const void *element)
{
    if (!list || !element) return SCL_ERR_NULL_PTR;

    scl_slist_node_t *node;
    scl_error_t err = scl_slist_create_node(alloc, &node, element, list->element_size);
    if (err != SCL_OK) return err;

    if (list->tail) {
        list->tail->next = node;
    } else {
        list->head = node;
    }
    list->tail = node;
    list->count++;
    return SCL_OK;
}

scl_error_t scl_slist_pop_front(scl_allocator_t *alloc, scl_slist_t *list, void *out)
{
    if (!list || !out) return SCL_ERR_NULL_PTR;
    if (!list->head) return SCL_ERR_EMPTY;

    scl_slist_node_t *node = list->head;
    memcpy(out, node->data, list->element_size);

    list->head = node->next;
    if (!list->head) list->tail = NULL;

    scl_free(alloc, node->data);
    scl_free(alloc, node);
    list->count--;
    return SCL_OK;
}

scl_error_t scl_slist_front(const scl_slist_t *list, void *out)
{
    if (!list || !out) return SCL_ERR_NULL_PTR;
    if (!list->head) return SCL_ERR_EMPTY;
    memcpy(out, list->head->data, list->element_size);
    return SCL_OK;
}

scl_error_t scl_slist_back(const scl_slist_t *list, void *out)
{
    if (!list || !out) return SCL_ERR_NULL_PTR;
    if (!list->tail) return SCL_ERR_EMPTY;
    memcpy(out, list->tail->data, list->element_size);
    return SCL_OK;
}

size_t scl_slist_count(const scl_slist_t *list)
{
    return list ? list->count : 0;
}

bool scl_slist_empty(const scl_slist_t *list)
{
    return list ? list->count == 0 : true;
}

scl_error_t scl_slist_remove(scl_allocator_t *alloc, scl_slist_t *list, const void *element,
                             int (*cmp)(const void *, const void *))
{
    if (!list || !element || !cmp) return SCL_ERR_NULL_PTR;

    scl_slist_node_t *prev = NULL;
    scl_slist_node_t *cur = list->head;

    while (cur) {
        if (cmp(cur->data, element) == 0) {
            if (prev)
                prev->next = cur->next;
            else
                list->head = cur->next;

            if (cur == list->tail)
                list->tail = prev;

            scl_free(alloc, cur->data);
            scl_free(alloc, cur);
            list->count--;
            return SCL_OK;
        }
        prev = cur;
        cur = cur->next;
    }
    return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_slist_search(const scl_slist_t *restrict list, const void *restrict key,
                             int (*cmp)(const void *, const void *),
                             void *restrict out)
{
    if (__builtin_expect(!list || !key || !cmp || !out, 0))
        return SCL_ERR_NULL_PTR;

    scl_slist_node_t *cur = list->head;
    while (__builtin_expect(cur != NULL, 1)) {
        if (cmp(cur->data, key) == 0) {
            memcpy(out, cur->data, list->element_size);
            return SCL_OK;
        }
        cur = cur->next;
    }
    return SCL_ERR_NOT_FOUND;
}
