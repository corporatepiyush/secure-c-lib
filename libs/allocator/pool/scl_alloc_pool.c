#include "scl_alloc_pool.h"
#include <string.h>
#include <stdalign.h>

typedef struct {
    scl_allocator_t *backing;
    void *chunk;
    void *free_list;
    size_t block_size;
    size_t total_blocks;
    size_t free_count;
} pool_state_t;

static void pool_free_fn(void *state, void *ptr);

static void *pool_malloc_fn(void *state, size_t size, size_t alignment) {
    (void)alignment;
    pool_state_t *p = (pool_state_t *)state;
    if (scl_unlikely(!p || size == 0 || size > p->block_size)) return NULL;
    if (scl_unlikely(!p->free_list)) return NULL;

    void *block = p->free_list;
    p->free_list = *(void **)block;
    p->free_count--;
    return block;
}

static void *pool_calloc_fn(void *state, size_t count, size_t size, size_t alignment) {
    size_t total;
    if (scl_unlikely(scl_mul_overflow(count, size, &total))) return NULL;
    void *ptr = pool_malloc_fn(state, total, alignment);
    if (scl_likely(ptr)) memset(ptr, 0, total);
    return ptr;
}

static void *pool_realloc_fn(void *state, void *ptr, size_t old_size, size_t new_size, size_t alignment) {
    if (scl_unlikely(!ptr)) return pool_malloc_fn(state, new_size, alignment);
    if (scl_unlikely(new_size == 0)) { pool_free_fn(state, ptr); return NULL; }
    void *new_ptr = pool_malloc_fn(state, new_size, alignment);
    if (scl_likely(new_ptr)) {
        size_t copy = old_size < new_size ? old_size : new_size;
        memcpy(new_ptr, ptr, copy);
        pool_free_fn(state, ptr);
    }
    return new_ptr;
}

static void pool_free_fn(void *state, void *ptr) {
    pool_state_t *p = (pool_state_t *)state;
    if (scl_unlikely(!p || !ptr)) return;

    unsigned char *start = (unsigned char *)p->chunk;
    unsigned char *end = start + p->block_size * p->total_blocks;
    if ((unsigned char *)ptr < start || (unsigned char *)ptr >= end)
        return;

    size_t offset = (unsigned char *)ptr - start;
    if (offset % p->block_size != 0) return;

    if (scl_unlikely(p->free_count >= p->total_blocks)) return;

    *(void **)ptr = p->free_list;
    p->free_list = ptr;
    p->free_count++;
}

scl_allocator_t *scl_alloc_pool_create(scl_allocator_t *backing, size_t block_size, size_t block_count, size_t alignment) {
    if (scl_unlikely(!backing || block_size == 0 || block_count == 0)) return NULL;
    (void)alignment;

    pool_state_t *state = (pool_state_t *)backing->malloc_fn(backing->state, sizeof(pool_state_t), alignof(max_align_t));
    if (scl_unlikely(!state)) return NULL;

    state->backing = backing;
    state->block_size = block_size;
    if (state->block_size < sizeof(void *))
        state->block_size = sizeof(void *);

    size_t total;
    if (scl_unlikely(scl_mul_overflow(state->block_size, block_count, &total))) {
        backing->free_fn(backing->state, state);
        return NULL;
    }

    state->chunk = backing->malloc_fn(backing->state, total, alignof(max_align_t));
    if (scl_unlikely(!state->chunk)) {
        backing->free_fn(backing->state, state);
        return NULL;
    }

    unsigned char *ptr = (unsigned char *)state->chunk;
    void *prev = NULL;
    for (size_t i = 0; i < block_count; i++) {
        void *block = ptr + i * state->block_size;
        *(void **)block = prev;
        prev = block;
    }

    state->free_list = prev;
    state->total_blocks = block_count;
    state->free_count = block_count;

    scl_allocator_t *alloc = (scl_allocator_t *)backing->malloc_fn(backing->state, sizeof(scl_allocator_t), alignof(max_align_t));
    if (scl_unlikely(!alloc)) {
        backing->free_fn(backing->state, state->chunk);
        backing->free_fn(backing->state, state);
        return NULL;
    }

    alloc->malloc_fn = pool_malloc_fn;
    alloc->calloc_fn = pool_calloc_fn;
    alloc->realloc_fn = pool_realloc_fn;
    alloc->free_fn = pool_free_fn;
    alloc->state = state;
    return alloc;
}

void scl_alloc_pool_destroy(scl_allocator_t *alloc) {
    if (scl_unlikely(!alloc)) return;
    pool_state_t *p = (pool_state_t *)alloc->state;
    scl_allocator_t *backing = p->backing;
    if (p->chunk) backing->free_fn(backing->state, p->chunk);
    backing->free_fn(backing->state, p);
    backing->free_fn(backing->state, alloc);
}
