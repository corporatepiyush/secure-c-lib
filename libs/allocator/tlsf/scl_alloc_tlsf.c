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

/* TLSF real-time allocator. 32 FL x 32 SL bins indexed via bitmap. Worst-case O(1) alloc/free. Suitable for hard real-time systems. */

#include "scl_alloc_tlsf.h"
#include <string.h>
#include <stdalign.h>
#include <limits.h>

#define TLSF_FL_MAX 32
#define TLSF_SL_COUNT 32
#define TLSF_SL_LOG2 5
#define TLSF_SMALL_BLOCK 256

typedef struct scl_tlsf_block_hdr {
    struct scl_tlsf_block_hdr *prev_phys;
    size_t size;
    struct scl_tlsf_block_hdr *next_free;
    struct scl_tlsf_block_hdr *prev_free;
} tlsf_block_hdr_t;

#define TLSF_BLOCK_HDR_SZ offsetof(tlsf_block_hdr_t, next_free)
#define TLSF_MIN_BLOCK_SIZE (TLSF_BLOCK_HDR_SZ + sizeof(void *))

typedef struct {
    scl_allocator_t *backing;
    void *pool;
    size_t pool_size;
    unsigned int fl_bitmap;
    unsigned int sl_bitmap[TLSF_FL_MAX];
    tlsf_block_hdr_t *bins[TLSF_FL_MAX][TLSF_SL_COUNT];
    tlsf_block_hdr_t *block_sentinel;
} tlsf_state_t;

static SCL_ALWAYS_INLINE SCL_PURE int tlsf_fls_size(size_t v) {
    if (v == 0) return 0;
    return (int)scl_log2_sz(v);
}

static SCL_ALWAYS_INLINE SCL_PURE int tlsf_ctz(unsigned int v) {
    return v ? (int)__builtin_ctz(v) : 0;
}

static SCL_ALWAYS_INLINE void tlsf_mapping(size_t size, int *fl, int *sl) {
    if (scl_likely(size < TLSF_SMALL_BLOCK)) {
        *fl = 0;
        *sl = (int)(size / (TLSF_SMALL_BLOCK / TLSF_SL_COUNT));
    } else {
        int s = tlsf_fls_size(size);
        *fl = s - (TLSF_SL_LOG2 - 1);
        *sl = (int)((size >> (s - TLSF_SL_LOG2)) ^ (1 << TLSF_SL_LOG2));
        if (scl_unlikely(*sl >= TLSF_SL_COUNT)) *sl = TLSF_SL_COUNT - 1;
    }
}

static SCL_ALWAYS_INLINE void tlsf_mapping_insert(size_t size, int *fl, int *sl) {
    if (scl_likely(size < TLSF_SMALL_BLOCK)) {
        *fl = 0;
        *sl = (int)(size / (TLSF_SMALL_BLOCK / TLSF_SL_COUNT));
    } else {
        int s = tlsf_fls_size(size);
        *fl = s - (TLSF_SL_LOG2 - 1);
        *sl = (int)((size >> (s - TLSF_SL_LOG2)) - (1 << TLSF_SL_LOG2));
    }
}

static void tlsf_remove_free(tlsf_state_t *tlsf, tlsf_block_hdr_t *block) {
    int fl, sl;
    tlsf_mapping_insert(block->size & ~3UL, &fl, &sl);

    if (block->next_free) block->next_free->prev_free = block->prev_free;
    if (block->prev_free) block->prev_free->next_free = block->next_free;

    if (tlsf->bins[fl][sl] == block) {
        tlsf->bins[fl][sl] = block->next_free;
        if (scl_unlikely(!tlsf->bins[fl][sl])) {
            tlsf->sl_bitmap[fl] &= ~(1U << sl);
            if (scl_unlikely(!tlsf->sl_bitmap[fl]))
                tlsf->fl_bitmap &= ~(1U << fl);
        }
    }

    block->next_free = NULL;
    block->prev_free = NULL;
}

static void tlsf_free_fn(void *state, void *ptr);

static void tlsf_insert_free(tlsf_state_t *tlsf, tlsf_block_hdr_t *block) {
    size_t size = block->size & ~3UL;
    int fl, sl;
    tlsf_mapping_insert(size, &fl, &sl);

    block->next_free = tlsf->bins[fl][sl];
    block->prev_free = NULL;
    if (scl_likely(tlsf->bins[fl][sl]))
        tlsf->bins[fl][sl]->prev_free = block;
    tlsf->bins[fl][sl] = block;

    tlsf->sl_bitmap[fl] |= (1U << sl);
    tlsf->fl_bitmap |= (1U << fl);
}

static int tlsf_search_and_remove(tlsf_state_t *tlsf, size_t size, tlsf_block_hdr_t **out) {
    int fl, sl;
    tlsf_mapping(size, &fl, &sl);

    unsigned int sl_bitmap = tlsf->sl_bitmap[fl];
    unsigned int sl_mask = sl_bitmap >> sl;
    if (scl_likely(sl_mask)) {
        sl += tlsf_ctz(sl_mask);
    } else {
        unsigned int fl_mask = tlsf->fl_bitmap >> (fl + 1);
        if (fl_mask) {
            fl += 1 + tlsf_ctz(fl_mask);
        } else {
            fl_mask = tlsf->fl_bitmap & ((1U << fl) - 1);
            if (fl_mask) {
                fl = (int)scl_log2_u32(fl_mask);
            } else {
                unsigned int all_above = tlsf->fl_bitmap & ~((1U << (fl + 1)) - 1);
                if (scl_unlikely(!all_above)) return 0;
                fl = tlsf_ctz(all_above);
            }
        }
        sl = tlsf_ctz(tlsf->sl_bitmap[fl]);
    }

    tlsf_block_hdr_t *block = tlsf->bins[fl][sl];
    if (scl_unlikely(!block)) return 0;

    tlsf_remove_free(tlsf, block);

    size_t block_size = block->size & ~3UL;
    size_t remaining = block_size - size;

    if (scl_likely(remaining >= TLSF_MIN_BLOCK_SIZE)) {
        tlsf_block_hdr_t *new_block = (tlsf_block_hdr_t *)((unsigned char *)block + size);
        new_block->prev_phys = block;
        new_block->size = remaining | 1;
        if (block->size & 2) new_block->size |= 2;

        block->size = size | 1 | (block->size & 2);

        tlsf_block_hdr_t *next_phys = (tlsf_block_hdr_t *)((unsigned char *)new_block + remaining);
        if ((unsigned char *)next_phys < (unsigned char *)tlsf->block_sentinel) {
            next_phys->prev_phys = new_block;
            if (next_phys->size & 1) {
                next_phys->size |= 2;
            }
        }

        tlsf_insert_free(tlsf, new_block);
    } else {
        block->size &= ~1UL;
        size = block_size;
    }

    tlsf_block_hdr_t *next = (tlsf_block_hdr_t *)((unsigned char *)block + size);
    if ((unsigned char *)next < (unsigned char *)tlsf->block_sentinel)
        next->size |= 2;

    *out = block;
    return 1;
}

static void *tlsf_malloc_fn(void *state, size_t size, size_t alignment) {
    tlsf_state_t *t = (tlsf_state_t *)state;
    if (scl_unlikely(!t || size == 0)) return NULL;
    (void)alignment;

    size_t aligned = size;
    size_t align = alignof(max_align_t);
    size_t mod = aligned % align;
    if (mod) aligned += align - mod;

    size_t req = TLSF_BLOCK_HDR_SZ + aligned;
    if (req < TLSF_MIN_BLOCK_SIZE) req = TLSF_MIN_BLOCK_SIZE;

    tlsf_block_hdr_t *block;
    if (scl_unlikely(!tlsf_search_and_remove(t, req, &block))) return NULL;

    return (void *)((unsigned char *)block + TLSF_BLOCK_HDR_SZ);
}

static void *tlsf_calloc_fn(void *state, size_t count, size_t size, size_t alignment) {
    size_t total;
    if (scl_unlikely(scl_mul_overflow(count, size, &total))) return NULL;
    void *ptr = tlsf_malloc_fn(state, total, alignment);
    if (scl_likely(ptr)) memset(ptr, 0, total);
    return ptr;
}

static void *tlsf_realloc_fn(void *state, void *ptr, size_t old_size, size_t new_size, size_t alignment) {
    if (scl_unlikely(!ptr)) return tlsf_malloc_fn(state, new_size, alignment);
    if (scl_unlikely(new_size == 0)) { tlsf_free_fn(state, ptr); return NULL; }
    void *new_ptr = tlsf_malloc_fn(state, new_size, alignment);
    if (scl_likely(new_ptr)) {
        size_t copy = old_size < new_size ? old_size : new_size;
        memcpy(new_ptr, ptr, copy);
        tlsf_free_fn(state, ptr);
    }
    return new_ptr;
}

static void tlsf_free_fn(void *state, void *ptr) {
    tlsf_state_t *t = (tlsf_state_t *)state;
    if (scl_unlikely(!t || !ptr)) return;

    tlsf_block_hdr_t *block = (tlsf_block_hdr_t *)((unsigned char *)ptr - TLSF_BLOCK_HDR_SZ);
    size_t block_size = block->size & ~3UL;

    int prev_free = (block->size & 2);
    if (scl_likely(prev_free && block->prev_phys)) {
        tlsf_block_hdr_t *prev = block->prev_phys;
        size_t prev_size = prev->size & ~3UL;
        tlsf_remove_free(t, prev);
        prev->size = (prev_size + block_size) | 1;
        block_size = prev_size + block_size;
        block = prev;
    }

    tlsf_block_hdr_t *next = (tlsf_block_hdr_t *)((unsigned char *)block + block_size);
    if (scl_likely((unsigned char *)next < (unsigned char *)t->block_sentinel &&
        (next->size & 1))) {
        size_t next_size = next->size & ~3UL;
        tlsf_remove_free(t, next);
        block->size = (block_size + next_size) | 1;
        block_size = block_size + next_size;
    }

    block->size = block_size | 1;
    if (block->prev_phys && (block->prev_phys->size & 1))
        block->prev_phys->size |= 2;

    tlsf_block_hdr_t *nxt = (tlsf_block_hdr_t *)((unsigned char *)block + block_size);
    if ((unsigned char *)nxt < (unsigned char *)t->block_sentinel) {
        nxt->prev_phys = block;
        nxt->size |= 2;
    }

    tlsf_insert_free(t, block);
}

scl_allocator_t *scl_alloc_tlsf_create(scl_allocator_t *backing, size_t memory_size) {
    if (scl_unlikely(!backing || memory_size < 4096)) return NULL;

    tlsf_state_t *state = (tlsf_state_t *)backing->malloc_fn(backing->state, sizeof(tlsf_state_t), alignof(max_align_t));
    if (scl_unlikely(!state)) return NULL;

    memset(state, 0, sizeof(tlsf_state_t));
    state->backing = backing;

    size_t actual = memory_size;
    size_t align = alignof(max_align_t);
    size_t mod = actual % align;
    if (mod != 0) actual += align - mod;

    state->pool = backing->malloc_fn(backing->state, actual, alignof(max_align_t));
    if (scl_unlikely(!state->pool)) {
        backing->free_fn(backing->state, state);
        return NULL;
    }
    state->pool_size = actual;

    unsigned char *ptr = (unsigned char *)state->pool;
    tlsf_block_hdr_t *start = (tlsf_block_hdr_t *)ptr;
    size_t block_sz = actual - TLSF_BLOCK_HDR_SZ;

    start->prev_phys = NULL;
    start->size = block_sz | 1;
    start->next_free = NULL;
    start->prev_free = NULL;

    state->block_sentinel = (tlsf_block_hdr_t *)(ptr + actual);
    state->block_sentinel->size = 0;
    state->block_sentinel->prev_phys = start;

    tlsf_insert_free(state, start);

    scl_allocator_t *alloc = (scl_allocator_t *)backing->malloc_fn(backing->state, sizeof(scl_allocator_t), alignof(max_align_t));
    if (scl_unlikely(!alloc)) {
        backing->free_fn(backing->state, state->pool);
        backing->free_fn(backing->state, state);
        return NULL;
    }

    alloc->malloc_fn = tlsf_malloc_fn;
    alloc->calloc_fn = tlsf_calloc_fn;
    alloc->realloc_fn = tlsf_realloc_fn;
    alloc->free_fn = tlsf_free_fn;
    alloc->state = state;
    return alloc;
}

void scl_alloc_tlsf_destroy(scl_allocator_t *alloc) {
    if (scl_unlikely(!alloc)) return;
    tlsf_state_t *s = (tlsf_state_t *)alloc->state;
    scl_allocator_t *backing = s->backing;
    if (s->pool) backing->free_fn(backing->state, s->pool);
    backing->free_fn(backing->state, s);
    backing->free_fn(backing->state, alloc);
}
