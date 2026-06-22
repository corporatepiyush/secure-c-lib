#include "scl_dlist.h"
#include <stdlib.h>
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

void scl_dlist_destroy(scl_dlist_t *list)
{
    if (!list) return;
    scl_dlist_node_t *cur = list->head;
    while (cur) {
        scl_dlist_node_t *next = cur->next;
        free(cur->data);
        free(cur);
        cur = next;
    }
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}

static scl_error_t scl_dlist_create_node(scl_dlist_node_t **out, const void *data, size_t element_size)
{
    scl_dlist_node_t *node = malloc(sizeof(scl_dlist_node_t));
    if (!node) return SCL_ERR_OUT_OF_MEMORY;

    node->data = malloc(element_size);
    if (!node->data) {
        free(node);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    memcpy(node->data, data, element_size);
    node->prev = NULL;
    node->next = NULL;
    *out = node;
    return SCL_OK;
}

scl_error_t scl_dlist_push_front(scl_dlist_t *list, const void *element)
{
    if (!list || !element) return SCL_ERR_NULL_PTR;

    scl_dlist_node_t *node;
    scl_error_t err = scl_dlist_create_node(&node, element, list->element_size);
    if (err != SCL_OK) return err;

    node->next = list->head;
    if (list->head)
        list->head->prev = node;
    else
        list->tail = node;
    list->head = node;
    list->count++;
    return SCL_OK;
}

scl_error_t scl_dlist_push_back(scl_dlist_t *list, const void *element)
{
    if (!list || !element) return SCL_ERR_NULL_PTR;

    scl_dlist_node_t *node;
    scl_error_t err = scl_dlist_create_node(&node, element, list->element_size);
    if (err != SCL_OK) return err;

    node->prev = list->tail;
    if (list->tail)
        list->tail->next = node;
    else
        list->head = node;
    list->tail = node;
    list->count++;
    return SCL_OK;
}

scl_error_t scl_dlist_pop_front(scl_dlist_t *list, void *out)
{
    if (!list || !out) return SCL_ERR_NULL_PTR;
    if (!list->head) return SCL_ERR_EMPTY;

    scl_dlist_node_t *node = list->head;
    memcpy(out, node->data, list->element_size);

    list->head = node->next;
    if (list->head)
        list->head->prev = NULL;
    else
        list->tail = NULL;

    free(node->data);
    free(node);
    list->count--;
    return SCL_OK;
}

scl_error_t scl_dlist_pop_back(scl_dlist_t *list, void *out)
{
    if (!list || !out) return SCL_ERR_NULL_PTR;
    if (!list->tail) return SCL_ERR_EMPTY;

    scl_dlist_node_t *node = list->tail;
    memcpy(out, node->data, list->element_size);

    list->tail = node->prev;
    if (list->tail)
        list->tail->next = NULL;
    else
        list->head = NULL;

    free(node->data);
    free(node);
    list->count--;
    return SCL_OK;
}

scl_error_t scl_dlist_front(const scl_dlist_t *list, void *out)
{
    if (!list || !out) return SCL_ERR_NULL_PTR;
    if (!list->head) return SCL_ERR_EMPTY;
    memcpy(out, list->head->data, list->element_size);
    return SCL_OK;
}

scl_error_t scl_dlist_back(const scl_dlist_t *list, void *out)
{
    if (!list || !out) return SCL_ERR_NULL_PTR;
    if (!list->tail) return SCL_ERR_EMPTY;
    memcpy(out, list->tail->data, list->element_size);
    return SCL_OK;
}

scl_error_t scl_dlist_insert_at(scl_dlist_t *list, size_t index, const void *element)
{
    if (!list || !element) return SCL_ERR_NULL_PTR;
    if (index > list->count) return SCL_ERR_INVALID_INDEX;

    if (index == 0) return scl_dlist_push_front(list, element);
    if (index == list->count) return scl_dlist_push_back(list, element);

    scl_dlist_node_t *cur;
    if (index <= list->count / 2) {
        cur = list->head;
        for (size_t i = 0; i < index; i++) cur = cur->next;
    } else {
        cur = list->tail;
        for (size_t i = list->count - 1; i > index; i--) cur = cur->prev;
    }

    scl_dlist_node_t *node;
    scl_error_t err = scl_dlist_create_node(&node, element, list->element_size);
    if (err != SCL_OK) return err;

    node->prev = cur->prev;
    node->next = cur;
    cur->prev->next = node;
    cur->prev = node;
    list->count++;
    return SCL_OK;
}

scl_error_t scl_dlist_remove_at(scl_dlist_t *list, size_t index, void *out)
{
    if (!list || !out) return SCL_ERR_NULL_PTR;
    if (index >= list->count) return SCL_ERR_INVALID_INDEX;

    if (index == 0) return scl_dlist_pop_front(list, out);
    if (index == list->count - 1) return scl_dlist_pop_back(list, out);

    scl_dlist_node_t *cur;
    if (index <= list->count / 2) {
        cur = list->head;
        for (size_t i = 0; i < index; i++) cur = cur->next;
    } else {
        cur = list->tail;
        for (size_t i = list->count - 1; i > index; i--) cur = cur->prev;
    }

    memcpy(out, cur->data, list->element_size);
    cur->prev->next = cur->next;
    cur->next->prev = cur->prev;
    free(cur->data);
    free(cur);
    list->count--;
    return SCL_OK;
}

scl_error_t scl_dlist_remove(scl_dlist_t *list, const void *element,
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

            free(cur->data);
            free(cur);
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
    if (__builtin_expect(!list || !key || !cmp || !out, 0))
        return SCL_ERR_NULL_PTR;

    scl_dlist_node_t *cur = list->head;
    while (__builtin_expect(cur != NULL, 1)) {
        if (cmp(cur->data, key) == 0) {
            memcpy(out, cur->data, list->element_size);
            return SCL_OK;
        }
        cur = cur->next;
    }
    return SCL_ERR_NOT_FOUND;
}

size_t scl_dlist_count(const scl_dlist_t *list)
{
    return list ? list->count : 0;
}

bool scl_dlist_empty(const scl_dlist_t *list)
{
    return list ? list->count == 0 : true;
}
