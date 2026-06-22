#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_alloc_buddy.h"
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>
#include <limits.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#define BUDDY_HDR_SZ sizeof(scl_buddy_node_t)
#define BUDDY_MIN_BLOCK (1UL << 4)  // 16 bytes min

static inline unsigned int buddy_order_for_size(size_t size) {
    unsigned int order = 0;
    size_t s = 1;
    while (s < size) { s <<= 1; order++; }
    return order;
}

static void *buddy_mmap_pool(size_t size) {
#ifdef _WIN32
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    return (p == MAP_FAILED) ? NULL : p;
#endif
}

static void buddy_munmap_pool(void *ptr, size_t size) {
#ifdef _WIN32
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, size);
#endif
}

scl_error_t scl_alloc_buddy_init(scl_alloc_buddy_t *buddy, unsigned int max_order) {
    if (__builtin_expect(!buddy, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(max_order == 0 || max_order > SCL_BUDDY_MAX_ORDER, 0))
        return SCL_ERR_INVALID_ARG;

    memset(buddy, 0, sizeof(*buddy));
    buddy->max_order = max_order;

    size_t pool_sz = (size_t)1 << max_order;
    if (pool_sz < BUDDY_MIN_BLOCK) pool_sz = BUDDY_MIN_BLOCK;

    unsigned char *pool = (unsigned char *)buddy_mmap_pool(pool_sz);
    if (__builtin_expect(!pool, 0)) return SCL_ERR_OUT_OF_MEMORY;
    buddy->pool = pool;
    buddy->pool_size = pool_sz;

    // Initialize the single largest block
    scl_buddy_node_t *node = (scl_buddy_node_t *)pool;
    node->next = NULL;
    node->order = max_order;
    buddy->free_lists[max_order] = node;

    return SCL_OK;
}

scl_error_t scl_alloc_buddy_alloc(scl_alloc_buddy_t *buddy, size_t size, void **out_ptr) {
    if (__builtin_expect(!buddy, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!out_ptr, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(size == 0, 0)) return SCL_ERR_INVALID_ARG;

    size_t actual = size + BUDDY_HDR_SZ;
    unsigned int order = buddy_order_for_size(actual);
    if (order > buddy->max_order) return SCL_ERR_INVALID_ARG;
    if (order < 4) order = 4;

    // Find the smallest available block >= order
    unsigned int current = order;
    while (current <= buddy->max_order && !buddy->free_lists[current])
        current++;

    if (__builtin_expect(current > buddy->max_order, 0))
        return SCL_ERR_OUT_OF_MEMORY;

    // Split until we get the right size
    while (current > order) {
        current--;
        scl_buddy_node_t *block = buddy->free_lists[current + 1];
        buddy->free_lists[current + 1] = block->next;

        size_t block_sz = (size_t)1 << current;
        unsigned char *block2 = (unsigned char *)block + block_sz;

        scl_buddy_node_t *buddy1 = (scl_buddy_node_t *)block;
        buddy1->next = buddy->free_lists[current];
        buddy1->order = current;

        scl_buddy_node_t *buddy2 = (scl_buddy_node_t *)block2;
        buddy2->next = NULL;
        buddy2->order = current;

        buddy->free_lists[current] = buddy2;
    }

    scl_buddy_node_t *node = buddy->free_lists[order];
    buddy->free_lists[order] = node->next;
    node->next = NULL;
    node->order = order;

    *out_ptr = (void *)((unsigned char *)node + BUDDY_HDR_SZ);
    return SCL_OK;
}

scl_error_t scl_alloc_buddy_free(scl_alloc_buddy_t *buddy, void *ptr) {
    if (__builtin_expect(!buddy, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!ptr, 0)) return SCL_ERR_NULL_PTR;

    scl_buddy_node_t *node = (scl_buddy_node_t *)((unsigned char *)ptr - BUDDY_HDR_SZ);
    unsigned int order = node->order;

    unsigned char *block = (unsigned char *)node;
    unsigned char *pool_start = buddy->pool;

    // Coalesce
    for (unsigned int o = order; o < buddy->max_order; o++) {
        size_t block_sz = (size_t)1 << o;
        unsigned long long idx = (block - pool_start) / block_sz;
        unsigned char *buddy_block = pool_start + (idx ^ 1) * block_sz;

        // Check if buddy is free and same order
        scl_buddy_node_t *prev = NULL;
        scl_buddy_node_t *cur = buddy->free_lists[o];
        int found = 0;
        while (cur) {
            if ((unsigned char *)cur == buddy_block && cur->order == o) {
                found = 1;
                break;
            }
            prev = cur;
            cur = cur->next;
        }

        if (!found) break;

        // Remove buddy from free list
        if (prev)
            prev->next = cur->next;
        else
            buddy->free_lists[o] = cur->next;

        // Merge: lower address becomes parent
        if (block > buddy_block) block = buddy_block;
        order = o + 1;
    }

    scl_buddy_node_t *merged = (scl_buddy_node_t *)block;
    merged->order = order;
    merged->next = buddy->free_lists[order];
    buddy->free_lists[order] = merged;

    return SCL_OK;
}

scl_error_t scl_alloc_buddy_destroy(scl_alloc_buddy_t *buddy) {
    if (__builtin_expect(!buddy, 0)) return SCL_ERR_NULL_PTR;
    if (buddy->pool) {
        buddy_munmap_pool(buddy->pool, buddy->pool_size);
        buddy->pool = NULL;
    }
    memset(buddy->free_lists, 0, sizeof(buddy->free_lists));
    buddy->pool_size = 0;
    buddy->max_order = 0;
    return SCL_OK;
}
