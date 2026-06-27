/*
 * Copyright 2026 Piyush Katariya
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Arena (bump) allocator. O(1) sequential allocation via pointer bump. Linked-list of chunks for bulk reset. Ideal for frame-scoped workloads. */

#include "scl_alloc_arena.h"
#include <string.h>
#include <stdalign.h>

typedef struct scl_arena_node {
    char *buffer;
    size_t offset;
    size_t capacity;
    size_t bytes_used;       /* total bytes actually handed out from this node */
    struct scl_arena_node *next;
} arena_node_t;

typedef struct {
    scl_allocator_t *backing;
    arena_node_t *head;
    arena_node_t *current;
    size_t max_capacity;     /* total backing memory limit (0 = no limit) */
    size_t total_allocated;  /* sum of all node capacities */
} arena_state_t;

static SCL_ALWAYS_INLINE SCL_PURE size_t arena_align_up(size_t offset, size_t align) {
    size_t mask = align - 1;
    return (offset + mask) & ~mask;
}

static arena_node_t *arena_node_create(arena_state_t *s, size_t capacity) {
    scl_allocator_t *b = s->backing;
    arena_node_t *node = (arena_node_t *)b->malloc_fn(b->state, sizeof(arena_node_t), alignof(max_align_t));
    if (scl_unlikely(!node)) return NULL;

    node->buffer = (char *)b->malloc_fn(b->state, capacity, alignof(max_align_t));
    if (scl_unlikely(!node->buffer)) {
        b->free_fn(b->state, node);
        return NULL;
    }

    node->offset = 0;
    node->capacity = capacity;
    node->bytes_used = 0;
    node->next = NULL;
    return node;
}

static void *arena_malloc_fn(void *state, size_t size, size_t alignment) {
    arena_state_t *s = (arena_state_t *)state;
    if (scl_unlikely(!s || size == 0)) return NULL;
    if (alignment == 0) alignment = alignof(max_align_t);

    arena_node_t *cur = s->current;
    size_t aligned = arena_align_up(cur->offset, alignment);
    if (scl_unlikely(aligned > cur->capacity || size > cur->capacity - aligned)) {
        size_t new_cap = cur->capacity * 2;
        while (new_cap < size + alignment)
            new_cap *= 2;

        if (scl_unlikely(s->max_capacity > 0 && s->total_allocated + new_cap > s->max_capacity))
            return NULL;

        arena_node_t *node = arena_node_create(s, new_cap);
        if (scl_unlikely(!node)) return NULL;

        node->next = cur->next;
        cur->next = node;
        s->current = node;
        s->total_allocated += new_cap;
        cur = node;
        aligned = 0;
    }

    void *ptr = cur->buffer + aligned;
    cur->offset = aligned + size;
    cur->bytes_used += size;
    return ptr;
}

static void *arena_calloc_fn(void *state, size_t count, size_t size, size_t alignment) {
    size_t total;
    if (scl_unlikely(scl_mul_overflow(count, size, &total))) return NULL;
    void *ptr = arena_malloc_fn(state, total, alignment);
    if (scl_likely(ptr)) memset(ptr, 0, total);
    return ptr;
}

static void *arena_realloc_fn(void *state, void *ptr, size_t old_size, size_t new_size, size_t alignment) {
    if (scl_unlikely(!ptr)) return arena_malloc_fn(state, new_size, alignment);
    if (scl_unlikely(new_size == 0)) return NULL;
    void *new_ptr = arena_malloc_fn(state, new_size, alignment);
    if (scl_likely(new_ptr)) {
        size_t copy = old_size < new_size ? old_size : new_size;
        memcpy(new_ptr, ptr, copy);
    }
    return new_ptr;
}

static void arena_free_fn(void *state, void *ptr) {
    (void)state;
    (void)ptr;
}

scl_allocator_t *scl_alloc_arena_create(scl_allocator_t *backing, size_t capacity, size_t max_capacity) {
    if (scl_unlikely(!backing || capacity == 0)) return NULL;

    arena_state_t *state = (arena_state_t *)backing->malloc_fn(backing->state, sizeof(arena_state_t), alignof(max_align_t));
    if (scl_unlikely(!state)) return NULL;

    state->backing = backing;
    state->max_capacity = max_capacity;
    state->total_allocated = 0;

    state->head = arena_node_create(state, capacity);
    if (scl_unlikely(!state->head)) {
        backing->free_fn(backing->state, state);
        return NULL;
    }
    state->total_allocated += capacity;
    state->current = state->head;

    scl_allocator_t *alloc = (scl_allocator_t *)backing->malloc_fn(backing->state, sizeof(scl_allocator_t), alignof(max_align_t));
    if (scl_unlikely(!alloc)) {
        backing->free_fn(backing->state, state->head->buffer);
        backing->free_fn(backing->state, state->head);
        backing->free_fn(backing->state, state);
        return NULL;
    }

    alloc->malloc_fn = arena_malloc_fn;
    alloc->calloc_fn = arena_calloc_fn;
    alloc->realloc_fn = arena_realloc_fn;
    alloc->free_fn = arena_free_fn;
    alloc->state = state;
    return alloc;
}

void scl_alloc_arena_reset(scl_allocator_t *alloc) {
    if (scl_unlikely(!alloc)) return;
    arena_state_t *s = (arena_state_t *)alloc->state;
    arena_node_t *cur = s->head;
    while (cur) {
#ifndef NDEBUG
        /* Poison free area in debug builds */
        memset(cur->buffer, 0xAA, cur->offset);
#endif
        cur->offset = 0;
        cur->bytes_used = 0;
        cur = cur->next;
    }
    s->current = s->head;
}

void scl_alloc_arena_destroy(scl_allocator_t *alloc) {
    if (scl_unlikely(!alloc)) return;
    arena_state_t *s = (arena_state_t *)alloc->state;
    scl_allocator_t *backing = s->backing;
    arena_node_t *cur = s->head;
    while (cur) {
        arena_node_t *next = cur->next;
#ifndef NDEBUG
        /* Poison entire buffer on destroy in debug builds */
        memset(cur->buffer, 0xBB, cur->capacity);
#endif
        backing->free_fn(backing->state, cur->buffer);
        backing->free_fn(backing->state, cur);
        cur = next;
    }
    backing->free_fn(backing->state, s);
    backing->free_fn(backing->state, alloc);
}

bool scl_alloc_arena_stats(scl_allocator_t *alloc, scl_alloc_arena_stats_t *out) {
    if (scl_unlikely(!alloc || !out)) return false;
    arena_state_t *s = (arena_state_t *)alloc->state;
    out->bytes_used = 0;
    out->bytes_wasted = 0;
    out->node_count = 0;
    arena_node_t *cur = s->head;
    while (cur) {
        out->bytes_used += cur->bytes_used;
        out->bytes_wasted += cur->capacity - cur->bytes_used;
        out->node_count++;
        cur = cur->next;
    }
    return true;
}
