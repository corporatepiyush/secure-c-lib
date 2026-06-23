#include "scl_concurrent_trie.h"
#include "scl_string.h"

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

static scl_concurrent_trie_node_t *create_node(scl_allocator_t *alloc)
{
    scl_concurrent_trie_node_t *n = scl_calloc(alloc, 1, sizeof(scl_concurrent_trie_node_t), alignof(max_align_t));
    if (!n) return NULL;
    n->value = NULL;
    n->terminal = false;
    atomic_flag_clear(&n->lock);
    return n;
}

void scl_ctrie_destroy(scl_allocator_t *alloc, scl_concurrent_trie_t *trie)
{
    if (!trie || !trie->root) return;

    scl_concurrent_trie_node_t *stack1[4096];
    int sp1 = 0;
    stack1[sp1++] = trie->root;

    scl_concurrent_trie_node_t *stack2[4096];
    int sp2 = 0;

    while (sp1 > 0) {
        scl_concurrent_trie_node_t *node = stack1[--sp1];
        stack2[sp2++] = node;
        for (int i = 0; i < SCL_TRIE_ALPHABET; i++) {
            if (node->children[i])
                stack1[sp1++] = node->children[i];
        }
    }

    while (sp2 > 0) {
        scl_concurrent_trie_node_t *node = stack2[--sp2];
        scl_free(alloc, node->value);
        scl_free(alloc, node);
    }

    trie->root = NULL;
    atomic_store_explicit(&trie->count, 0, memory_order_relaxed);
}

scl_error_t scl_ctrie_init(scl_allocator_t *alloc, scl_concurrent_trie_t *trie, size_t value_size)
{
    if (!trie) return SCL_ERR_NULL_PTR;
    if (value_size == 0) return SCL_ERR_INVALID_ARG;
    trie->root = create_node(alloc);
    if (!trie->root) return SCL_ERR_OUT_OF_MEMORY;
    trie->value_size = value_size;
    atomic_init(&trie->count, 0);
    return SCL_OK;
}

scl_error_t scl_ctrie_insert(scl_allocator_t *alloc, scl_concurrent_trie_t *trie, const unsigned char *key,
                            size_t key_len, const void *value)
{
    if (!trie || !key || !value) return SCL_ERR_NULL_PTR;
    scl_concurrent_trie_node_t *cur = trie->root;
    for (size_t i = 0; i < key_len; i++) {
        unsigned char c = key[i];
        spin_lock(&cur->lock);
        if (!cur->children[c]) {
            scl_concurrent_trie_node_t *n = create_node(alloc);
            if (!n) { spin_unlock(&cur->lock); return SCL_ERR_OUT_OF_MEMORY; }
            cur->children[c] = n;
        }
        if (i < key_len - 1) {
            scl_concurrent_trie_node_t *next = cur->children[c];
            spin_unlock(&cur->lock);
            cur = next;
        } else {
            scl_concurrent_trie_node_t *next = cur->children[c];
            spin_lock(&next->lock);
            bool was_terminal = next->terminal;
            if (!next->value) {
                next->value = scl_alloc(alloc, trie->value_size, alignof(max_align_t));
                if (!next->value) { spin_unlock(&next->lock); spin_unlock(&cur->lock); return SCL_ERR_OUT_OF_MEMORY; }
            }
            scl_memcpy(next->value, value, trie->value_size);
            next->terminal = true;
            if (!was_terminal) atomic_fetch_add_explicit(&trie->count, 1, memory_order_relaxed);
            spin_unlock(&next->lock);
            spin_unlock(&cur->lock);
            return SCL_OK;
        }
    }
    spin_lock(&cur->lock);
    if (!cur->terminal) {
        if (!cur->value) {
            cur->value = scl_alloc(alloc, trie->value_size, alignof(max_align_t));
            if (!cur->value) { spin_unlock(&cur->lock); return SCL_ERR_OUT_OF_MEMORY; }
        }
        scl_memcpy(cur->value, value, trie->value_size);
        cur->terminal = true;
        atomic_fetch_add_explicit(&trie->count, 1, memory_order_relaxed);
    } else {
        scl_memcpy(cur->value, value, trie->value_size);
    }
    spin_unlock(&cur->lock);
    return SCL_OK;
}

scl_error_t scl_ctrie_get(const scl_concurrent_trie_t *trie, const unsigned char *key,
                         size_t key_len, void *out_value)
{
    if (!trie || !key || !out_value) return SCL_ERR_NULL_PTR;
    scl_concurrent_trie_node_t *cur = trie->root;
    spin_lock(&cur->lock);
    scl_concurrent_trie_node_t *prev = NULL;
    for (size_t i = 0; i < key_len; i++) {
        unsigned char c = key[i];
        scl_concurrent_trie_node_t *next = cur->children[c];
        if (!next) { if (prev) spin_unlock(&prev->lock); spin_unlock(&cur->lock); return SCL_ERR_NOT_FOUND; }
        spin_lock(&next->lock);
        if (prev) spin_unlock(&prev->lock);
        prev = cur;
        cur = next;
    }
    if (prev) spin_unlock(&prev->lock);
    if (cur->terminal) {
        scl_memcpy(out_value, cur->value, trie->value_size);
        spin_unlock(&cur->lock);
        return SCL_OK;
    }
    spin_unlock(&cur->lock);
    return SCL_ERR_NOT_FOUND;
}

bool scl_ctrie_contains(const scl_concurrent_trie_t *trie, const unsigned char *key,
                       size_t key_len)
{
    if (!trie || !key) return false;
    scl_concurrent_trie_node_t *cur = trie->root;
    spin_lock(&cur->lock);
    scl_concurrent_trie_node_t *prev = NULL;
    for (size_t i = 0; i < key_len; i++) {
        unsigned char c = key[i];
        scl_concurrent_trie_node_t *next = cur->children[c];
        if (!next) { if (prev) spin_unlock(&prev->lock); spin_unlock(&cur->lock); return false; }
        spin_lock(&next->lock);
        if (prev) spin_unlock(&prev->lock);
        prev = cur;
        cur = next;
    }
    if (prev) spin_unlock(&prev->lock);
    bool result = cur->terminal;
    spin_unlock(&cur->lock);
    return result;
}

scl_error_t scl_ctrie_remove(scl_allocator_t *alloc, scl_concurrent_trie_t *trie, const unsigned char *key,
                            size_t key_len)
{
    if (!trie || !key) return SCL_ERR_NULL_PTR;
    if (key_len == 0) return SCL_ERR_INVALID_ARG;

    scl_concurrent_trie_node_t *path[1024];
    int path_len = 0;
    scl_concurrent_trie_node_t *cur = trie->root;
    path[path_len++] = cur;
    spin_lock(&cur->lock);
    scl_concurrent_trie_node_t *prev = NULL;

    for (size_t i = 0; i < key_len; i++) {
        unsigned char c = key[i];
        scl_concurrent_trie_node_t *next = cur->children[c];
        if (!next) { if (prev) spin_unlock(&prev->lock); spin_unlock(&cur->lock); return SCL_ERR_NOT_FOUND; }
        spin_lock(&next->lock);
        if (prev) spin_unlock(&prev->lock);
        path[path_len++] = next;
        prev = cur;
        cur = next;
    }

    if (prev) spin_unlock(&prev->lock);
    if (!cur->terminal) { spin_unlock(&cur->lock); return SCL_ERR_NOT_FOUND; }
    cur->terminal = false;
    scl_free(alloc, cur->value);
    cur->value = NULL;
    atomic_fetch_sub_explicit(&trie->count, 1, memory_order_relaxed);

    bool can_free = true;
    for (int i = 0; i < SCL_TRIE_ALPHABET && can_free; i++)
        if (cur->children[i]) can_free = false;

    if (can_free) {
        unsigned char c = key[key_len - 1];
        scl_concurrent_trie_node_t *parent = path[path_len - 2];
        spin_lock(&parent->lock);
        parent->children[c] = NULL;
        spin_unlock(&parent->lock);
        spin_unlock(&cur->lock);
        scl_free(alloc, cur);
    } else {
        spin_unlock(&cur->lock);
    }

    return SCL_OK;
}

size_t scl_ctrie_count(const scl_concurrent_trie_t *trie)
{
    return trie ? atomic_load_explicit(&trie->count, memory_order_relaxed) : 0;
}
