#include "concurrent_trie.h"
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

static scl_concurrent_trie_node_t *create_node(void)
{
    scl_concurrent_trie_node_t *n = calloc(1, sizeof(scl_concurrent_trie_node_t));
    if (!n) return NULL;
    n->value = NULL;
    n->terminal = false;
    atomic_flag_clear(&n->lock);
    return n;
}

static void destroy_subtree(scl_concurrent_trie_node_t *n)
{
    if (!n) return;
    for (int i = 0; i < SCL_CONCURRENT_TRIE_ALPHABET; i++) {
        if (n->children[i]) destroy_subtree(n->children[i]);
    }
    free(n->value);
    free(n);
}

scl_error_t scl_concurrent_trie_init(scl_concurrent_trie_t *trie, size_t value_size)
{
    if (!trie) return SCL_ERR_NULL_PTR;
    if (value_size == 0) return SCL_ERR_INVALID_ARG;
    trie->root = create_node();
    if (!trie->root) return SCL_ERR_OUT_OF_MEMORY;
    trie->value_size = value_size;
    atomic_init(&trie->count, 0);
    return SCL_OK;
}

void scl_concurrent_trie_destroy(scl_concurrent_trie_t *trie)
{
    if (!trie) return;
    destroy_subtree(trie->root);
    trie->root = NULL;
    atomic_store_explicit(&trie->count, 0, memory_order_relaxed);
}

scl_error_t scl_concurrent_trie_insert(scl_concurrent_trie_t *trie, const unsigned char *key,
                                       size_t key_len, const void *value)
{
    if (!trie || !key || !value) return SCL_ERR_NULL_PTR;
    scl_concurrent_trie_node_t *cur = trie->root;
    for (size_t i = 0; i < key_len; i++) {
        unsigned char c = key[i];
        spin_lock(&cur->lock);
        if (!cur->children[c]) {
            scl_concurrent_trie_node_t *n = create_node();
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
                next->value = malloc(trie->value_size);
                if (!next->value) { spin_unlock(&next->lock); spin_unlock(&cur->lock); return SCL_ERR_OUT_OF_MEMORY; }
            }
            memcpy(next->value, value, trie->value_size);
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
            cur->value = malloc(trie->value_size);
            if (!cur->value) { spin_unlock(&cur->lock); return SCL_ERR_OUT_OF_MEMORY; }
        }
        memcpy(cur->value, value, trie->value_size);
        cur->terminal = true;
        atomic_fetch_add_explicit(&trie->count, 1, memory_order_relaxed);
    } else {
        memcpy(cur->value, value, trie->value_size);
    }
    spin_unlock(&cur->lock);
    return SCL_OK;
}

scl_error_t scl_concurrent_trie_get(const scl_concurrent_trie_t *trie, const unsigned char *key,
                                    size_t key_len, void *out_value)
{
    if (!trie || !key || !out_value) return SCL_ERR_NULL_PTR;
    scl_concurrent_trie_node_t *cur = trie->root;
    for (size_t i = 0; i < key_len; i++) {
        unsigned char c = key[i];
        if (!cur->children[c]) return SCL_ERR_NOT_FOUND;
        cur = cur->children[c];
    }
    spin_lock(&cur->lock);
    if (cur->terminal) {
        memcpy(out_value, cur->value, trie->value_size);
        spin_unlock(&cur->lock);
        return SCL_OK;
    }
    spin_unlock(&cur->lock);
    return SCL_ERR_NOT_FOUND;
}

bool scl_concurrent_trie_contains(const scl_concurrent_trie_t *trie, const unsigned char *key,
                                  size_t key_len)
{
    if (!trie || !key) return false;
    scl_concurrent_trie_node_t *cur = trie->root;
    for (size_t i = 0; i < key_len; i++) {
        unsigned char c = key[i];
        if (!cur->children[c]) return false;
        cur = cur->children[c];
    }
    spin_lock(&cur->lock);
    bool result = cur->terminal;
    spin_unlock(&cur->lock);
    return result;
}

scl_error_t scl_concurrent_trie_remove(scl_concurrent_trie_t *trie, const unsigned char *key,
                                       size_t key_len)
{
    if (!trie || !key) return SCL_ERR_NULL_PTR;
    if (key_len == 0) return SCL_ERR_INVALID_ARG;
    spin_lock(&trie->root->lock);
    bool found = false;
    scl_concurrent_trie_node_t *cur = trie->root;
    for (size_t i = 0; i < key_len; i++) {
        unsigned char c = key[i];
        if (!cur->children[c]) { spin_unlock(&trie->root->lock); return SCL_ERR_NOT_FOUND; }
        if (i < key_len - 1) cur = cur->children[c];
    }
    scl_concurrent_trie_node_t *target = cur->children[key[key_len - 1]];
    if (!target || !target->terminal) { spin_unlock(&trie->root->lock); return SCL_ERR_NOT_FOUND; }
    found = true;
    target->terminal = false;
    free(target->value);
    target->value = NULL;
    atomic_fetch_sub_explicit(&trie->count, 1, memory_order_relaxed);
    bool can_free = true;
    for (int i = 0; i < SCL_CONCURRENT_TRIE_ALPHABET && can_free; i++)
        if (target->children[i]) can_free = false;
    if (can_free) {
        free(target);
        cur->children[key[key_len - 1]] = NULL;
    }
    spin_unlock(&trie->root->lock);
    return found ? SCL_OK : SCL_ERR_NOT_FOUND;
}

size_t scl_concurrent_trie_count(const scl_concurrent_trie_t *trie)
{
    return trie ? atomic_load_explicit(&trie->count, memory_order_relaxed) : 0;
}
