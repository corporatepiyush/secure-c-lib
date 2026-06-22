#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_alloc_tlsf.h"
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>
#include <limits.h>

#define TLSF_BLOCK_HDR_SZ offsetof(scl_tlsf_block_hdr_t, next_free)
#define TLSF_MIN_BLOCK_SIZE (TLSF_BLOCK_HDR_SZ + sizeof(void *))
#define TLSF_FLI_SHIFT 5

static inline int tlsf_fls_size(size_t v) {
    if (v == 0) return 0;
    return (int)(sizeof(size_t) * CHAR_BIT - 1 - __builtin_clzll((unsigned long long)v));
}

static inline void tlsf_mapping(size_t size, int *fl, int *sl) {
    if (size < SCL_TLSF_SMALL_BLOCK) {
        *fl = 0;
        *sl = (int)(size / (SCL_TLSF_SMALL_BLOCK / SCL_TLSF_SL_COUNT));
    } else {
        int s = tlsf_fls_size(size);
        *fl = s - (SCL_TLSF_SL_LOG2 - 1);
        *sl = (int)((size >> (s - SCL_TLSF_SL_LOG2)) ^ (1 << SCL_TLSF_SL_LOG2));
        if (*sl >= SCL_TLSF_SL_COUNT) *sl = SCL_TLSF_SL_COUNT - 1;
    }
}

static inline void tlsf_mapping_insert(size_t size, int *fl, int *sl) {
    if (size < SCL_TLSF_SMALL_BLOCK) {
        *fl = 0;
        *sl = (int)(size / (SCL_TLSF_SMALL_BLOCK / SCL_TLSF_SL_COUNT));
    } else {
        int s = tlsf_fls_size(size);
        *fl = s - (SCL_TLSF_SL_LOG2 - 1);
        *sl = (int)((size >> (s - SCL_TLSF_SL_LOG2)) - (1 << SCL_TLSF_SL_LOG2));
    }
}

static void tlsf_remove_free(scl_alloc_tlsf_t *tlsf, scl_tlsf_block_hdr_t *block) {
    int fl, sl;
    tlsf_mapping_insert(block->size & ~3UL, &fl, &sl);

    if (block->next_free) block->next_free->prev_free = block->prev_free;
    if (block->prev_free) block->prev_free->next_free = block->next_free;

    if (tlsf->bins[fl][sl] == block) {
        tlsf->bins[fl][sl] = block->next_free;
        if (!tlsf->bins[fl][sl]) {
            tlsf->sl_bitmap[fl] &= ~(1U << sl);
            if (!tlsf->sl_bitmap[fl])
                tlsf->fl_bitmap &= ~(1U << fl);
        }
    }

    block->next_free = NULL;
    block->prev_free = NULL;
}

static void tlsf_insert_free(scl_alloc_tlsf_t *tlsf, scl_tlsf_block_hdr_t *block) {
    size_t size = block->size & ~3UL;
    int fl, sl;
    tlsf_mapping_insert(size, &fl, &sl);

    block->next_free = tlsf->bins[fl][sl];
    block->prev_free = NULL;
    if (tlsf->bins[fl][sl])
        tlsf->bins[fl][sl]->prev_free = block;
    tlsf->bins[fl][sl] = block;

    tlsf->sl_bitmap[fl] |= (1U << sl);
    tlsf->fl_bitmap |= (1U << fl);
}

static inline int tlsf_ctz(unsigned int v) {
    return v ? __builtin_ctz(v) : 0;
}

static inline int tlsf_clz(unsigned int v) {
    return v ? __builtin_clz(v) : sizeof(unsigned int) * CHAR_BIT;
}

static scl_error_t tlsf_search_and_remove(scl_alloc_tlsf_t *tlsf, size_t size, scl_tlsf_block_hdr_t **out) {
    int fl, sl;
    tlsf_mapping(size, &fl, &sl);

    unsigned int sl_bitmap = tlsf->sl_bitmap[fl];
    unsigned int sl_mask = sl_bitmap >> sl;
    if (sl_mask) {
        sl += tlsf_ctz(sl_mask);
    } else {
        unsigned int fl_mask = tlsf->fl_bitmap >> (fl + 1);
        if (fl_mask) {
            fl += 1 + tlsf_ctz(fl_mask);
        } else {
            fl_mask = tlsf->fl_bitmap & ((1U << fl) - 1);
            if (fl_mask) {
                fl = (int)(sizeof(unsigned int) * CHAR_BIT - 1 - tlsf_clz(fl_mask));
            } else {
                unsigned int all_above = tlsf->fl_bitmap & ~((1U << (fl + 1)) - 1);
                if (!all_above) return SCL_ERR_OUT_OF_MEMORY;
                fl = tlsf_ctz(all_above);
            }
        }
        sl = tlsf_ctz(tlsf->sl_bitmap[fl]);
    }

    scl_tlsf_block_hdr_t *block = tlsf->bins[fl][sl];
    if (__builtin_expect(!block, 0)) return SCL_ERR_OUT_OF_MEMORY;

    tlsf_remove_free(tlsf, block);

    size_t block_size = block->size & ~3UL;
    size_t remaining = block_size - size;

    if (remaining >= TLSF_MIN_BLOCK_SIZE) {
        scl_tlsf_block_hdr_t *new_block = (scl_tlsf_block_hdr_t *)((unsigned char *)block + size);
        new_block->prev_phys = block;
        new_block->size = remaining | 1;
        if (block->size & 2) new_block->size |= 2;

        block->size = size | 1 | (block->size & 2);

        scl_tlsf_block_hdr_t *next_phys = (scl_tlsf_block_hdr_t *)((unsigned char *)new_block + remaining);
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

    scl_tlsf_block_hdr_t *next = (scl_tlsf_block_hdr_t *)((unsigned char *)block + size);
    if ((unsigned char *)next < (unsigned char *)tlsf->block_sentinel)
        next->size |= 2;

    *out = block;
    return SCL_OK;
}

scl_error_t scl_alloc_tlsf_init(scl_alloc_tlsf_t *tlsf, size_t pool_size) {
    if (__builtin_expect(!tlsf, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(pool_size < 4096, 0)) return SCL_ERR_INVALID_ARG;

    memset(tlsf, 0, sizeof(*tlsf));

    size_t actual = pool_size;
    size_t align = alignof(max_align_t);
    if (actual % align != 0) actual += align - (actual % align);

    tlsf->pool = malloc(actual);
    if (__builtin_expect(!tlsf->pool, 0)) return SCL_ERR_OUT_OF_MEMORY;
    tlsf->pool_size = actual;

    unsigned char *ptr = (unsigned char *)tlsf->pool;
    scl_tlsf_block_hdr_t *start = (scl_tlsf_block_hdr_t *)ptr;
    size_t block_sz = actual - TLSF_BLOCK_HDR_SZ;

    start->prev_phys = NULL;
    start->size = block_sz | 1;
    start->next_free = NULL;
    start->prev_free = NULL;

    tlsf->block_sentinel = (scl_tlsf_block_hdr_t *)(ptr + actual);
    tlsf->block_sentinel->size = 0;
    tlsf->block_sentinel->prev_phys = start;

    tlsf_insert_free(tlsf, start);

    return SCL_OK;
}

scl_error_t scl_alloc_tlsf_alloc(scl_alloc_tlsf_t *tlsf, size_t size, void **out_ptr) {
    if (__builtin_expect(!tlsf, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!out_ptr, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(size == 0, 0)) size = 1;

    size_t aligned = (size + alignof(max_align_t) - 1) & ~(alignof(max_align_t) - 1);
    size_t req = TLSF_BLOCK_HDR_SZ + aligned;
    if (req < TLSF_MIN_BLOCK_SIZE) req = TLSF_MIN_BLOCK_SIZE;

    scl_tlsf_block_hdr_t *block;
    scl_error_t err = tlsf_search_and_remove(tlsf, req, &block);
    if (__builtin_expect(err != SCL_OK, 0)) return err;

    *out_ptr = (void *)((unsigned char *)block + TLSF_BLOCK_HDR_SZ);
    return SCL_OK;
}

scl_error_t scl_alloc_tlsf_free(scl_alloc_tlsf_t *tlsf, void *ptr) {
    if (__builtin_expect(!tlsf, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!ptr, 0)) return SCL_ERR_NULL_PTR;

    scl_tlsf_block_hdr_t *block = (scl_tlsf_block_hdr_t *)((unsigned char *)ptr - TLSF_BLOCK_HDR_SZ);
    size_t block_size = block->size & ~3UL;

    // Coalesce with previous if free
    int prev_free = (block->size & 2);
    if (prev_free && block->prev_phys) {
        scl_tlsf_block_hdr_t *prev = block->prev_phys;
        size_t prev_size = prev->size & ~3UL;
        tlsf_remove_free(tlsf, prev);
        prev->size = (prev_size + block_size) | 1;
        block_size = prev_size + block_size;
        block = prev;
    }

    // Coalesce with next if free
    scl_tlsf_block_hdr_t *next = (scl_tlsf_block_hdr_t *)((unsigned char *)block + block_size);
    if ((unsigned char *)next < (unsigned char *)tlsf->block_sentinel &&
        (next->size & 1)) {
        size_t next_size = next->size & ~3UL;
        tlsf_remove_free(tlsf, next);
        block->size = (block_size + next_size) | 1;
        block_size = block_size + next_size;
    }

    block->size = block_size | 1;
    if (block->prev_phys && (block->prev_phys->size & 1))
        block->prev_phys->size |= 2;

    scl_tlsf_block_hdr_t *nxt = (scl_tlsf_block_hdr_t *)((unsigned char *)block + block_size);
    if ((unsigned char *)nxt < (unsigned char *)tlsf->block_sentinel) {
        nxt->prev_phys = block;
        nxt->size |= 2;
    }

    tlsf_insert_free(tlsf, block);
    return SCL_OK;
}

scl_error_t scl_alloc_tlsf_destroy(scl_alloc_tlsf_t *tlsf) {
    if (__builtin_expect(!tlsf, 0)) return SCL_ERR_NULL_PTR;
    free(tlsf->pool);
    memset(tlsf, 0, sizeof(*tlsf));
    return SCL_OK;
}
