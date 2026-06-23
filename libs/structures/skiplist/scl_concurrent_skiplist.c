#include "scl_concurrent_skiplist.h"
#include "scl_string.h"
#include <time.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static inline void spin_lock(atomic_flag *lock)
{
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {
        scl_cpu_pause();
    }
}

static inline void spin_unlock(atomic_flag *lock)
{
    atomic_flag_clear_explicit(lock, memory_order_release);
}

static size_t random_level(void)
{
    size_t lvl = 1;
    while ((((double)rand() / RAND_MAX) < 0.5) && lvl < SCL_SKIPLIST_MAX_LEVEL)
        lvl++;
    return lvl;
}

static scl_concurrent_skiplist_node_t *create_node(scl_allocator_t *alloc, const void *data, size_t element_size, size_t level)
{
    scl_concurrent_skiplist_node_t *n = scl_alloc(alloc, sizeof(scl_concurrent_skiplist_node_t), alignof(max_align_t));
    if (!n) return NULL;
    n->data = scl_alloc(alloc, element_size, alignof(max_align_t));
    if (!n->data) { scl_free(alloc, n); return NULL; }
    scl_memcpy(n->data, data, element_size);
    n->forward = scl_calloc(alloc, level, sizeof(scl_concurrent_skiplist_node_t *), alignof(max_align_t));
    if (!n->forward) { scl_free(alloc, n->data); scl_free(alloc, n); return NULL; }
    atomic_init(&n->level, level);
    return n;
}

scl_error_t scl_cskiplist_init(scl_allocator_t *alloc, scl_concurrent_skiplist_t *sl, size_t element_size,
                              scl_cmp_func_t cmp)
{
    if (!sl) return SCL_ERR_NULL_PTR;
    if (element_size == 0 || !cmp) return SCL_ERR_INVALID_ARG;
    sl->head = scl_alloc(alloc, sizeof(scl_concurrent_skiplist_node_t), alignof(max_align_t));
    if (!sl->head) return SCL_ERR_OUT_OF_MEMORY;
    sl->head->data = NULL;
    sl->head->forward = scl_calloc(alloc, SCL_SKIPLIST_MAX_LEVEL, sizeof(scl_concurrent_skiplist_node_t *), alignof(max_align_t));
    if (!sl->head->forward) { scl_free(alloc, sl->head); return SCL_ERR_OUT_OF_MEMORY; }
    atomic_init(&sl->head->level, SCL_SKIPLIST_MAX_LEVEL);
    sl->element_size = element_size;
    atomic_init(&sl->count, 0);
    sl->cmp = cmp;
    atomic_init(&sl->level, 1);
    atomic_flag_clear(&sl->lock);
    srand((unsigned int)time(NULL));
    return SCL_OK;
}

void scl_cskiplist_destroy(scl_allocator_t *alloc, scl_concurrent_skiplist_t *sl)
{
    if (!sl) return;
    scl_concurrent_skiplist_node_t *cur = sl->head;
    while (cur) {
        scl_concurrent_skiplist_node_t *next = cur->forward[0];
        scl_free(alloc, cur->data);
        scl_free(alloc, cur->forward);
        scl_free(alloc, cur);
        cur = next;
    }
    atomic_store_explicit(&sl->count, 0, memory_order_relaxed);
}

scl_error_t scl_cskiplist_insert(scl_allocator_t *alloc, scl_concurrent_skiplist_t *sl, const void *element)
{
    if (!sl || !element) return SCL_ERR_NULL_PTR;
    spin_lock(&sl->lock);
    scl_concurrent_skiplist_node_t *update[SCL_SKIPLIST_MAX_LEVEL];
    scl_concurrent_skiplist_node_t *cur = sl->head;
    for (size_t i = atomic_load_explicit(&sl->level, memory_order_relaxed); i > 0; i--) {
        size_t idx = i - 1;
        while (cur->forward[idx] && sl->cmp(cur->forward[idx]->data, element) < 0)
            cur = cur->forward[idx];
        update[idx] = cur;
    }
    cur = cur->forward[0];
    if (cur && sl->cmp(cur->data, element) == 0) {
        scl_memcpy(cur->data, element, sl->element_size);
        spin_unlock(&sl->lock);
        return SCL_OK;
    }
    size_t lvl = random_level();
    if (lvl > atomic_load_explicit(&sl->level, memory_order_relaxed)) {
        for (size_t i = atomic_load_explicit(&sl->level, memory_order_relaxed); i < lvl; i++)
            update[i] = sl->head;
        atomic_store_explicit(&sl->level, lvl, memory_order_relaxed);
    }
    scl_concurrent_skiplist_node_t *n = create_node(alloc, element, sl->element_size, lvl);
    if (!n) { spin_unlock(&sl->lock); return SCL_ERR_OUT_OF_MEMORY; }
    for (size_t i = 0; i < lvl; i++) {
        n->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = n;
    }
    atomic_fetch_add_explicit(&sl->count, 1, memory_order_relaxed);
    spin_unlock(&sl->lock);
    return SCL_OK;
}

scl_error_t scl_cskiplist_remove(scl_allocator_t *alloc, scl_concurrent_skiplist_t *sl, const void *key)
{
    if (!sl || !key) return SCL_ERR_NULL_PTR;
    spin_lock(&sl->lock);
    scl_concurrent_skiplist_node_t *update[SCL_SKIPLIST_MAX_LEVEL];
    scl_concurrent_skiplist_node_t *cur = sl->head;
    for (size_t i = atomic_load_explicit(&sl->level, memory_order_relaxed); i > 0; i--) {
        size_t idx = i - 1;
        while (cur->forward[idx] && sl->cmp(cur->forward[idx]->data, key) < 0)
            cur = cur->forward[idx];
        update[idx] = cur;
    }
    cur = cur->forward[0];
    if (!cur || sl->cmp(cur->data, key) != 0) {
        spin_unlock(&sl->lock);
        return SCL_ERR_NOT_FOUND;
    }
    size_t lvl = atomic_load_explicit(&cur->level, memory_order_relaxed);
    for (size_t i = 0; i < lvl; i++)
        update[i]->forward[i] = cur->forward[i];
    scl_free(alloc, cur->data);
    scl_free(alloc, cur->forward);
    scl_free(alloc, cur);
    while (atomic_load_explicit(&sl->level, memory_order_relaxed) > 1 &&
           !sl->head->forward[atomic_load_explicit(&sl->level, memory_order_relaxed) - 1])
        atomic_fetch_sub_explicit(&sl->level, 1, memory_order_relaxed);
    atomic_fetch_sub_explicit(&sl->count, 1, memory_order_relaxed);
    spin_unlock(&sl->lock);
    return SCL_OK;
}

bool scl_cskiplist_contains(const scl_concurrent_skiplist_t *sl, const void *key)
{
    if (!sl || !key) return false;
    spin_lock((atomic_flag *)&sl->lock);
    scl_concurrent_skiplist_node_t *cur = sl->head;
    for (size_t i = atomic_load_explicit(&sl->level, memory_order_relaxed); i > 0; i--) {
        size_t idx = i - 1;
        while (cur->forward[idx] && sl->cmp(cur->forward[idx]->data, key) < 0)
            cur = cur->forward[idx];
    }
    cur = cur->forward[0];
    bool found = cur && sl->cmp(cur->data, key) == 0;
    spin_unlock((atomic_flag *)&sl->lock);
    return found;
}

scl_error_t scl_cskiplist_find(const scl_concurrent_skiplist_t *sl, const void *key, void *out)
{
    if (!sl || !key || !out) return SCL_ERR_NULL_PTR;
    spin_lock((atomic_flag *)&sl->lock);
    scl_concurrent_skiplist_node_t *cur = sl->head;
    for (size_t i = atomic_load_explicit(&sl->level, memory_order_relaxed); i > 0; i--) {
        size_t idx = i - 1;
        while (cur->forward[idx] && sl->cmp(cur->forward[idx]->data, key) < 0)
            cur = cur->forward[idx];
    }
    cur = cur->forward[0];
    if (cur && sl->cmp(cur->data, key) == 0) {
        scl_memcpy(out, cur->data, sl->element_size);
        spin_unlock((atomic_flag *)&sl->lock);
        return SCL_OK;
    }
    spin_unlock((atomic_flag *)&sl->lock);
    return SCL_ERR_NOT_FOUND;
}

size_t scl_cskiplist_count(const scl_concurrent_skiplist_t *sl)
{
    return sl ? atomic_load_explicit(&sl->count, memory_order_relaxed) : 0;
}
