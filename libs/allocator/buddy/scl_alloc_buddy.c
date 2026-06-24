#include "scl_alloc_buddy.h"
#include <string.h>
#include <stdalign.h>
#include <limits.h>

#define BUDDY_MAX_ORDER 20
#define BUDDY_HDR_SZ sizeof(scl_buddy_node_t)
#define BUDDY_MIN_BLOCK (1UL << 4)

typedef struct scl_buddy_node {
    struct scl_buddy_node *next;
    unsigned int order;
} scl_buddy_node_t;

typedef struct {
    scl_allocator_t *backing;
    unsigned char *pool;
    size_t pool_size;
    unsigned int max_order;
    scl_buddy_node_t *free_lists[BUDDY_MAX_ORDER + 1];
} buddy_state_t;

static SCL_ALWAYS_INLINE SCL_PURE unsigned int buddy_order_for_size(size_t size) {
    unsigned int order = 0;
    size_t s = 1;
    while (s < size) { s <<= 1; order++; }
    return order;
}

static void buddy_free_fn(void *state, void *ptr);

static void *buddy_malloc_fn(void *state, size_t size, size_t alignment) {
    (void)alignment;
    buddy_state_t *b = (buddy_state_t *)state;
    if (scl_unlikely(!b || size == 0)) return NULL;

    size_t actual = size + BUDDY_HDR_SZ;
    unsigned int order = buddy_order_for_size(actual);
    if (scl_unlikely(order > b->max_order)) return NULL;
    if (order < 4) order = 4;

    unsigned int current = order;
    while (current <= b->max_order && !b->free_lists[current])
        current++;

    if (scl_unlikely(current > b->max_order)) return NULL;

    while (current > order) {
        current--;
        scl_buddy_node_t *block = b->free_lists[current + 1];
        b->free_lists[current + 1] = block->next;

        size_t block_sz = (size_t)1 << current;
        unsigned char *block2 = (unsigned char *)block + block_sz;

        scl_buddy_node_t *buddy1 = (scl_buddy_node_t *)block;
        buddy1->next = b->free_lists[current];
        buddy1->order = current;

        scl_buddy_node_t *buddy2 = (scl_buddy_node_t *)block2;
        buddy2->next = NULL;
        buddy2->order = current;

        b->free_lists[current] = buddy2;
    }

    scl_buddy_node_t *node = b->free_lists[order];
    b->free_lists[order] = node->next;
    node->next = NULL;
    node->order = order;

    return (void *)((unsigned char *)node + BUDDY_HDR_SZ);
}

static void *buddy_calloc_fn(void *state, size_t count, size_t size, size_t alignment) {
    size_t total;
    if (scl_unlikely(scl_mul_overflow(count, size, &total))) return NULL;
    void *ptr = buddy_malloc_fn(state, total, alignment);
    if (scl_likely(ptr)) memset(ptr, 0, total);
    return ptr;
}

static void *buddy_realloc_fn(void *state, void *ptr, size_t old_size, size_t new_size, size_t alignment) {
    if (scl_unlikely(!ptr)) return buddy_malloc_fn(state, new_size, alignment);
    if (scl_unlikely(new_size == 0)) { buddy_free_fn(state, ptr); return NULL; }
    void *new_ptr = buddy_malloc_fn(state, new_size, alignment);
    if (scl_likely(new_ptr)) {
        size_t copy = old_size < new_size ? old_size : new_size;
        memcpy(new_ptr, ptr, copy);
        buddy_free_fn(state, ptr);
    }
    return new_ptr;
}

static void buddy_free_fn(void *state, void *ptr) {
    buddy_state_t *b = (buddy_state_t *)state;
    if (scl_unlikely(!b || !ptr)) return;

    scl_buddy_node_t *node = (scl_buddy_node_t *)((unsigned char *)ptr - BUDDY_HDR_SZ);
    unsigned int order = node->order;

    unsigned char *block = (unsigned char *)node;
    unsigned char *pool_start = b->pool;

    for (unsigned int o = order; o < b->max_order; o++) {
        size_t block_sz = (size_t)1 << o;
        unsigned long long idx = (block - pool_start) / block_sz;
        unsigned char *buddy_block = pool_start + (idx ^ 1) * block_sz;

        scl_buddy_node_t *prev = NULL;
        scl_buddy_node_t *cur = b->free_lists[o];
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

        if (prev)
            prev->next = cur->next;
        else
            b->free_lists[o] = cur->next;

        if (block > buddy_block) block = buddy_block;
        order = o + 1;
    }

    scl_buddy_node_t *merged = (scl_buddy_node_t *)block;
    merged->order = order;
    merged->next = b->free_lists[order];
    b->free_lists[order] = merged;
}

scl_allocator_t *scl_alloc_buddy_create(scl_allocator_t *backing, size_t total_size) {
    if (scl_unlikely(!backing || total_size == 0)) return NULL;

    size_t pool_sz = scl_bit_ceil_sz(total_size);
    if (pool_sz < BUDDY_MIN_BLOCK) pool_sz = BUDDY_MIN_BLOCK;

    unsigned int max_order = (unsigned int)scl_log2_sz(pool_sz);
    if (scl_unlikely(max_order > BUDDY_MAX_ORDER)) return NULL;

    buddy_state_t *state = (buddy_state_t *)backing->malloc_fn(backing->state, sizeof(buddy_state_t), alignof(max_align_t));
    if (scl_unlikely(!state)) return NULL;

    memset(state, 0, sizeof(buddy_state_t));
    state->backing = backing;
    state->max_order = max_order;

    state->pool = (unsigned char *)backing->malloc_fn(backing->state, pool_sz, alignof(max_align_t));
    if (scl_unlikely(!state->pool)) {
        backing->free_fn(backing->state, state);
        return NULL;
    }
    state->pool_size = pool_sz;

    scl_buddy_node_t *node = (scl_buddy_node_t *)state->pool;
    node->next = NULL;
    node->order = max_order;
    state->free_lists[max_order] = node;

    scl_allocator_t *alloc = (scl_allocator_t *)backing->malloc_fn(backing->state, sizeof(scl_allocator_t), alignof(max_align_t));
    if (scl_unlikely(!alloc)) {
        backing->free_fn(backing->state, state->pool);
        backing->free_fn(backing->state, state);
        return NULL;
    }

    alloc->malloc_fn = buddy_malloc_fn;
    alloc->calloc_fn = buddy_calloc_fn;
    alloc->realloc_fn = buddy_realloc_fn;
    alloc->free_fn = buddy_free_fn;
    alloc->state = state;
    return alloc;
}

void scl_alloc_buddy_destroy(scl_allocator_t *alloc) {
    if (scl_unlikely(!alloc)) return;
    buddy_state_t *b = (buddy_state_t *)alloc->state;
    scl_allocator_t *backing = b->backing;
    if (b->pool) backing->free_fn(backing->state, b->pool);
    backing->free_fn(backing->state, b);
    backing->free_fn(backing->state, alloc);
}
