#include "scl_dlist.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_dlist_init(scl_dlist_t *list, size_t element_size)
{
    if (!list) return SCL_ERR_NULL_PTR;
    if (element_size == 0) return SCL_ERR_INVALID_ARG;
    list->head = NULL;
    list->tail = NULL;
    list->element_size = element_size;
    list->count = 0;
    return SCL_OK;
}

void scl_dlist_destroy(scl_allocator_t *alloc, scl_dlist_t *list)
{
    if (!list) return;
    scl_dlist_node_t *cur = list->head;
    while (cur) {
        scl_dlist_node_t *next = cur->next;
        scl_free(alloc, cur);
        cur = next;
    }
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}

static scl_error_t scl_dlist_create_node(scl_allocator_t *alloc, scl_dlist_node_t **out,
                                          const void *data, size_t element_size)
{
    size_t node_sz;
    if (scl_add_overflow(sizeof(scl_dlist_node_t), element_size, &node_sz))
        return SCL_ERR_SIZE_OVERFLOW;
    scl_dlist_node_t *node = scl_alloc(alloc, node_sz, alignof(max_align_t));
    if (!node) return SCL_ERR_OUT_OF_MEMORY;
    memcpy(node->data, data, element_size);
    node->prev = NULL;
    node->next = NULL;
    *out = node;
    return SCL_OK;
}

scl_error_t scl_dlist_push_front(scl_allocator_t *alloc, scl_dlist_t *list, const void *element)
{
    if (!list || !element) return SCL_ERR_NULL_PTR;
    size_t esz = list->element_size;
    scl_dlist_node_t *node;
    scl_error_t err = scl_dlist_create_node(alloc, &node, element, esz);
    if (scl_unlikely(err != SCL_OK)) return err;
    node->next = list->head;
    if (scl_likely(list->head != NULL))
        list->head->prev = node;
    else
        list->tail = node;
    list->head = node;
    list->count++;
    return SCL_OK;
}

scl_error_t scl_dlist_push_back(scl_allocator_t *alloc, scl_dlist_t *list, const void *element)
{
    if (!list || !element) return SCL_ERR_NULL_PTR;
    size_t esz = list->element_size;
    scl_dlist_node_t *node;
    scl_error_t err = scl_dlist_create_node(alloc, &node, element, esz);
    if (scl_unlikely(err != SCL_OK)) return err;
    node->prev = list->tail;
    if (scl_likely(list->tail != NULL))
        list->tail->next = node;
    else
        list->head = node;
    list->tail = node;
    list->count++;
    return SCL_OK;
}

scl_error_t scl_dlist_pop_front(scl_allocator_t *alloc, scl_dlist_t *list, void *out)
{
    if (!list || !out) return SCL_ERR_NULL_PTR;
    scl_dlist_node_t *node = list->head;
    if (!node) return SCL_ERR_EMPTY;
    size_t esz = list->element_size;
    memcpy(out, node->data, esz);
    list->head = node->next;
    if (list->head)
        list->head->prev = NULL;
    else
        list->tail = NULL;
    scl_free(alloc, node);
    list->count--;
    return SCL_OK;
}

scl_error_t scl_dlist_pop_back(scl_allocator_t *alloc, scl_dlist_t *list, void *out)
{
    if (!list || !out) return SCL_ERR_NULL_PTR;
    scl_dlist_node_t *node = list->tail;
    if (!node) return SCL_ERR_EMPTY;
    size_t esz = list->element_size;
    memcpy(out, node->data, esz);
    list->tail = node->prev;
    if (list->tail)
        list->tail->next = NULL;
    else
        list->head = NULL;
    scl_free(alloc, node);
    list->count--;
    return SCL_OK;
}

scl_error_t scl_dlist_front(const scl_dlist_t *list, void *out)
{
    if (!list || !out) return SCL_ERR_NULL_PTR;
    scl_dlist_node_t *node = list->head;
    if (!node) return SCL_ERR_EMPTY;
    memcpy(out, node->data, list->element_size);
    return SCL_OK;
}

scl_error_t scl_dlist_back(const scl_dlist_t *list, void *out)
{
    if (!list || !out) return SCL_ERR_NULL_PTR;
    scl_dlist_node_t *node = list->tail;
    if (!node) return SCL_ERR_EMPTY;
    memcpy(out, node->data, list->element_size);
    return SCL_OK;
}

scl_error_t scl_dlist_insert_at(scl_allocator_t *alloc, scl_dlist_t *list, size_t index, const void *element)
{
    if (!list || !element) return SCL_ERR_NULL_PTR;
    if (index > list->count) return SCL_ERR_INVALID_INDEX;
    if (index == 0) return scl_dlist_push_front(alloc, list, element);
    size_t cnt = list->count;
    if (index == cnt) return scl_dlist_push_back(alloc, list, element);
    size_t esz = list->element_size;
    scl_dlist_node_t *cur;
    if (index <= cnt / 2) {
        cur = list->head;
        for (size_t i = 0; i < index; i++) cur = cur->next;
    } else {
        cur = list->tail;
        for (size_t i = cnt - 1; i > index; i--) cur = cur->prev;
    }
    scl_dlist_node_t *node;
    scl_error_t err = scl_dlist_create_node(alloc, &node, element, esz);
    if (scl_unlikely(err != SCL_OK)) return err;
    node->prev = cur->prev;
    node->next = cur;
    cur->prev->next = node;
    cur->prev = node;
    list->count++;
    return SCL_OK;
}

scl_error_t scl_dlist_remove_at(scl_allocator_t *alloc, scl_dlist_t *list, size_t index, void *out)
{
    if (!list || !out) return SCL_ERR_NULL_PTR;
    if (index >= list->count) return SCL_ERR_INVALID_INDEX;
    if (index == 0) return scl_dlist_pop_front(alloc, list, out);
    size_t cnt = list->count;
    if (index == cnt - 1) return scl_dlist_pop_back(alloc, list, out);
    size_t esz = list->element_size;
    scl_dlist_node_t *cur;
    if (index <= cnt / 2) {
        cur = list->head;
        for (size_t i = 0; i < index; i++) cur = cur->next;
    } else {
        cur = list->tail;
        for (size_t i = cnt - 1; i > index; i--) cur = cur->prev;
    }
    memcpy(out, cur->data, esz);
    cur->prev->next = cur->next;
    cur->next->prev = cur->prev;
    scl_free(alloc, cur);
    list->count--;
    return SCL_OK;
}

scl_error_t scl_dlist_remove(scl_allocator_t *alloc, scl_dlist_t *list, const void *element,
                             int (*cmp)(const void *, const void *))
{
    if (!list || !element || !cmp) return SCL_ERR_NULL_PTR;
    scl_dlist_node_t *cur = list->head;
    while (cur) {
        if (cmp(cur->data, element) == 0) {
            if (cur->prev)
                cur->prev->next = cur->next;
            else
                list->head = cur->next;
            if (cur->next)
                cur->next->prev = cur->prev;
            else
                list->tail = cur->prev;
            scl_free(alloc, cur);
            list->count--;
            return SCL_OK;
        }
        cur = cur->next;
    }
    return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_dlist_search(const scl_dlist_t *restrict list, const void *restrict key,
                             int (*cmp)(const void *, const void *),
                             void *restrict out)
{
    if (scl_unlikely(!list || !key || !cmp || !out))
        return SCL_ERR_NULL_PTR;

    size_t esz = list->element_size;
    scl_dlist_node_t *cur = list->head;
    while (scl_likely(cur != NULL)) {
        if (cmp(cur->data, key) == 0) {
            memcpy(out, cur->data, esz);
            return SCL_OK;
        }
        cur = cur->next;
    }
    return SCL_ERR_NOT_FOUND;
}

size_t scl_dlist_count(const scl_dlist_t *list) { return list ? list->count : 0; }
bool scl_dlist_empty(const scl_dlist_t *list) { return list ? list->count == 0 : true; }
