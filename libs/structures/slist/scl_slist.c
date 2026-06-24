#include "scl_slist.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_slist_init(scl_slist_t *list, size_t element_size)
{
    if (scl_unlikely(!list)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(element_size == 0)) return SCL_ERR_INVALID_ARG;
    list->head = NULL;
    list->tail = NULL;
    list->element_size = element_size;
    list->count = 0;
    return SCL_OK;
}

void scl_slist_destroy(scl_allocator_t *alloc, scl_slist_t *list)
{
    if (scl_unlikely(!list)) return;
    scl_slist_node_t *cur = list->head;
    while (scl_likely(cur)) {
        scl_slist_node_t *next = cur->next;
        scl_free(alloc, cur);
        cur = next;
    }
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}

static scl_error_t scl_slist_create_node(scl_allocator_t *alloc, scl_slist_node_t **out,
                                          const void *data, size_t element_size)
{
    size_t node_sz;
    if (scl_add_overflow(sizeof(scl_slist_node_t), element_size, &node_sz))
        return SCL_ERR_SIZE_OVERFLOW;
    scl_slist_node_t *node = scl_alloc(alloc, node_sz, alignof(max_align_t));
    if (scl_unlikely(!node)) return SCL_ERR_OUT_OF_MEMORY;
    scl_memcpy(node->data, data, element_size);
    node->next = NULL;
    *out = node;
    return SCL_OK;
}

scl_error_t scl_slist_push_front(scl_allocator_t *alloc, scl_slist_t *list, const void  *SCL_RESTRICT element)
{
    if (scl_unlikely(!list || !element)) return SCL_ERR_NULL_PTR;
    size_t esz = list->element_size;
    scl_slist_node_t *node;
    scl_error_t err = scl_slist_create_node(alloc, &node, element, esz);
    if (scl_unlikely(err != SCL_OK)) return err;
    node->next = list->head;
    list->head = node;
    if (!list->tail) list->tail = node;
    list->count++;
    return SCL_OK;
}

scl_error_t scl_slist_push_back(scl_allocator_t *alloc, scl_slist_t *list, const void  *SCL_RESTRICT element)
{
    if (scl_unlikely(!list || !element)) return SCL_ERR_NULL_PTR;
    size_t esz = list->element_size;
    scl_slist_node_t *node;
    scl_error_t err = scl_slist_create_node(alloc, &node, element, esz);
    if (scl_unlikely(err != SCL_OK)) return err;
    if (scl_likely(list->tail != NULL))
        list->tail->next = node;
    else
        list->head = node;
    list->tail = node;
    list->count++;
    return SCL_OK;
}

scl_error_t scl_slist_pop_front(scl_allocator_t *alloc, scl_slist_t *list, void  *SCL_RESTRICT out)
{
    if (scl_unlikely(!list || !out)) return SCL_ERR_NULL_PTR;
    scl_slist_node_t *node = list->head;
    if (scl_unlikely(!node)) return SCL_ERR_EMPTY;
    size_t esz = list->element_size;
    scl_memcpy(out, node->data, esz);
    list->head = node->next;
    if (!list->head) list->tail = NULL;
    scl_free(alloc, node);
    list->count--;
    return SCL_OK;
}

scl_error_t scl_slist_front(const scl_slist_t *list, void *out)
{
    if (scl_unlikely(!list || !out)) return SCL_ERR_NULL_PTR;
    scl_slist_node_t *node = list->head;
    if (scl_unlikely(!node)) return SCL_ERR_EMPTY;
    scl_memcpy(out, node->data, list->element_size);
    return SCL_OK;
}

scl_error_t scl_slist_back(const scl_slist_t *list, void *out)
{
    if (scl_unlikely(!list || !out)) return SCL_ERR_NULL_PTR;
    scl_slist_node_t *node = list->tail;
    if (scl_unlikely(!node)) return SCL_ERR_EMPTY;
    scl_memcpy(out, node->data, list->element_size);
    return SCL_OK;
}

size_t scl_slist_count(const scl_slist_t *list) { return list ? list->count : 0; }
bool scl_slist_empty(const scl_slist_t *list) { return list ? list->count == 0 : true; }

scl_error_t scl_slist_remove(scl_allocator_t *alloc, scl_slist_t *list, const void *element,
                             int (*cmp)(const void *, const void *))
{
    if (scl_unlikely(!list || !element || !cmp)) return SCL_ERR_NULL_PTR;
    scl_slist_node_t *prev = NULL;
    scl_slist_node_t *cur = list->head;
    while (scl_likely(cur)) {
        if (cmp(cur->data, element) == 0) {
            if (prev)
                prev->next = cur->next;
            else
                list->head = cur->next;
            if (cur == list->tail)
                list->tail = prev;
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
    if (scl_unlikely(!list || !key || !cmp || !out))
        return SCL_ERR_NULL_PTR;
    size_t esz = list->element_size;
    scl_slist_node_t *cur = list->head;
    while (scl_likely(cur != NULL)) {
        if (cmp(cur->data, key) == 0) {
            scl_memcpy(out, cur->data, esz);
            return SCL_OK;
        }
        cur = cur->next;
    }
    return SCL_ERR_NOT_FOUND;
}
