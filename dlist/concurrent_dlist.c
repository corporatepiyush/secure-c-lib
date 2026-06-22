#include "concurrent_dlist.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static inline void spin_lock(atomic_flag *lock)
{
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {
        __asm__ volatile("yield");
    }
}

static inline void spin_unlock(atomic_flag *lock)
{
    atomic_flag_clear_explicit(lock, memory_order_release);
}

static scl_error_t create_node(scl_concurrent_dlist_node_t **out, const void *data, size_t element_size)
{
    scl_concurrent_dlist_node_t *node = malloc(sizeof(scl_concurrent_dlist_node_t));
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

scl_error_t scl_concurrent_dlist_init(scl_concurrent_dlist_t *list, size_t element_size)
{
    if (!list) return SCL_ERR_NULL_PTR;
    if (element_size == 0) return SCL_ERR_INVALID_ARG;
    list->head = NULL;
    list->tail = NULL;
    list->element_size = element_size;
    atomic_init(&list->count, 0);
    atomic_flag_clear(&list->lock);
    return SCL_OK;
}

void scl_concurrent_dlist_destroy(scl_concurrent_dlist_t *list)
{
    if (!list) return;
    spin_lock(&list->lock);
    scl_concurrent_dlist_node_t *cur = list->head;
    while (cur) {
        scl_concurrent_dlist_node_t *next = cur->next;
        free(cur->data);
        free(cur);
        cur = next;
    }
    list->head = NULL;
    list->tail = NULL;
    atomic_store_explicit(&list->count, 0, memory_order_relaxed);
    spin_unlock(&list->lock);
}

scl_error_t scl_concurrent_dlist_push_front(scl_concurrent_dlist_t *list, const void *element)
{
    if (!list || !element) return SCL_ERR_NULL_PTR;
    scl_concurrent_dlist_node_t *node;
    scl_error_t err = create_node(&node, element, list->element_size);
    if (err != SCL_OK) return err;

    spin_lock(&list->lock);
    node->next = list->head;
    if (list->head)
        list->head->prev = node;
    else
        list->tail = node;
    list->head = node;
    atomic_fetch_add_explicit(&list->count, 1, memory_order_relaxed);
    spin_unlock(&list->lock);
    return SCL_OK;
}

scl_error_t scl_concurrent_dlist_push_back(scl_concurrent_dlist_t *list, const void *element)
{
    if (!list || !element) return SCL_ERR_NULL_PTR;
    scl_concurrent_dlist_node_t *node;
    scl_error_t err = create_node(&node, element, list->element_size);
    if (err != SCL_OK) return err;

    spin_lock(&list->lock);
    node->prev = list->tail;
    if (list->tail)
        list->tail->next = node;
    else
        list->head = node;
    list->tail = node;
    atomic_fetch_add_explicit(&list->count, 1, memory_order_relaxed);
    spin_unlock(&list->lock);
    return SCL_OK;
}

scl_error_t scl_concurrent_dlist_pop_front(scl_concurrent_dlist_t *list, void *out)
{
    if (!list || !out) return SCL_ERR_NULL_PTR;
    spin_lock(&list->lock);
    if (!list->head) {
        spin_unlock(&list->lock);
        return SCL_ERR_EMPTY;
    }
    scl_concurrent_dlist_node_t *node = list->head;
    memcpy(out, node->data, list->element_size);
    list->head = node->next;
    if (list->head)
        list->head->prev = NULL;
    else
        list->tail = NULL;
    atomic_fetch_sub_explicit(&list->count, 1, memory_order_relaxed);
    spin_unlock(&list->lock);
    free(node->data);
    free(node);
    return SCL_OK;
}

scl_error_t scl_concurrent_dlist_pop_back(scl_concurrent_dlist_t *list, void *out)
{
    if (!list || !out) return SCL_ERR_NULL_PTR;
    spin_lock(&list->lock);
    if (!list->tail) {
        spin_unlock(&list->lock);
        return SCL_ERR_EMPTY;
    }
    scl_concurrent_dlist_node_t *node = list->tail;
    memcpy(out, node->data, list->element_size);
    list->tail = node->prev;
    if (list->tail)
        list->tail->next = NULL;
    else
        list->head = NULL;
    atomic_fetch_sub_explicit(&list->count, 1, memory_order_relaxed);
    spin_unlock(&list->lock);
    free(node->data);
    free(node);
    return SCL_OK;
}

scl_error_t scl_concurrent_dlist_insert_at(scl_concurrent_dlist_t *list, size_t index, const void *element)
{
    if (!list || !element) return SCL_ERR_NULL_PTR;
    scl_concurrent_dlist_node_t *node;
    scl_error_t err = create_node(&node, element, list->element_size);
    if (err != SCL_OK) return err;

    spin_lock(&list->lock);
    size_t cnt = atomic_load_explicit(&list->count, memory_order_relaxed);
    if (index > cnt) {
        spin_unlock(&list->lock);
        free(node->data);
        free(node);
        return SCL_ERR_INVALID_INDEX;
    }
    if (index == 0) {
        node->next = list->head;
        if (list->head) list->head->prev = node;
        else list->tail = node;
        list->head = node;
    } else if (index == cnt) {
        node->prev = list->tail;
        if (list->tail) list->tail->next = node;
        else list->head = node;
        list->tail = node;
    } else {
        scl_concurrent_dlist_node_t *cur = list->head;
        for (size_t i = 0; i < index; i++) cur = cur->next;
        node->prev = cur->prev;
        node->next = cur;
        cur->prev->next = node;
        cur->prev = node;
    }
    atomic_fetch_add_explicit(&list->count, 1, memory_order_relaxed);
    spin_unlock(&list->lock);
    return SCL_OK;
}

scl_error_t scl_concurrent_dlist_remove_at(scl_concurrent_dlist_t *list, size_t index, void *out)
{
    if (!list || !out) return SCL_ERR_NULL_PTR;
    spin_lock(&list->lock);
    size_t cnt = atomic_load_explicit(&list->count, memory_order_relaxed);
    if (index >= cnt) {
        spin_unlock(&list->lock);
        return SCL_ERR_INVALID_INDEX;
    }
    scl_concurrent_dlist_node_t *cur;
    if (index == 0) {
        cur = list->head;
        list->head = cur->next;
        if (list->head) list->head->prev = NULL;
        else list->tail = NULL;
    } else if (index == cnt - 1) {
        cur = list->tail;
        list->tail = cur->prev;
        if (list->tail) list->tail->next = NULL;
        else list->head = NULL;
    } else {
        cur = list->head;
        for (size_t i = 0; i < index; i++) cur = cur->next;
        cur->prev->next = cur->next;
        cur->next->prev = cur->prev;
    }
    memcpy(out, cur->data, list->element_size);
    atomic_fetch_sub_explicit(&list->count, 1, memory_order_relaxed);
    spin_unlock(&list->lock);
    free(cur->data);
    free(cur);
    return SCL_OK;
}

size_t scl_concurrent_dlist_count(const scl_concurrent_dlist_t *list)
{
    return list ? atomic_load_explicit(&list->count, memory_order_relaxed) : 0;
}

bool scl_concurrent_dlist_empty(const scl_concurrent_dlist_t *list)
{
    return list ? atomic_load_explicit(&list->count, memory_order_relaxed) == 0 : true;
}
