#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_alloc_slab.h"
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>

const size_t scl_alloc_slab_sizes[SCL_ALLOC_SLAB_CLASSES] = {
    8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096
};

static inline size_t scl_align_up(size_t size, size_t align) {
    size_t mod = size % align;
    return mod ? size + (align - mod) : size;
}

static size_t slab_class_for_size(size_t size) {
    if (size <= 8) return 0;
    if (size <= 16) return 1;
    if (size <= 32) return 2;
    if (size <= 64) return 3;
    if (size <= 128) return 4;
    if (size <= 256) return 5;
    if (size <= 512) return 6;
    if (size <= 1024) return 7;
    if (size <= 2048) return 8;
    if (size <= 4096) return 9;
    return SCL_ALLOC_SLAB_CLASSES;
}

static scl_error_t slab_pool_init(scl_alloc_slab_pool_t *p, size_t block_size) {
    size_t blocks = block_size <= 32 ? 1024 :
                    block_size <= 128 ? 512 :
                    block_size <= 512 ? 256 : 128;

    size_t aligned = scl_align_up(block_size, alignof(max_align_t));
    if (aligned < sizeof(void *)) aligned = sizeof(void *);

    size_t total;
    if (scl_mul_overflow(aligned, blocks, &total))
        return SCL_ERR_SIZE_OVERFLOW;

    void *chunk = malloc(total);
    if (!chunk) return SCL_ERR_OUT_OF_MEMORY;

    unsigned char *ptr = (unsigned char *)chunk;
    void *prev = NULL;
    for (size_t i = 0; i < blocks; i++) {
        void *block = ptr + i * aligned;
        *(void **)block = prev;
        prev = block;
    }

    p->chunk = chunk;
    p->free_list = prev;
    p->block_size = aligned;
    p->total_blocks = blocks;
    p->free_count = blocks;
    return SCL_OK;
}

scl_error_t scl_alloc_slab_init(scl_alloc_slab_t *slab) {
    if (__builtin_expect(!slab, 0)) return SCL_ERR_NULL_PTR;
    for (int i = 0; i < SCL_ALLOC_SLAB_CLASSES; i++) {
        scl_error_t err = slab_pool_init(&slab->pools[i], scl_alloc_slab_sizes[i]);
        if (__builtin_expect(err != SCL_OK, 0)) {
            for (int j = 0; j < i; j++) {
                free(slab->pools[j].chunk);
                slab->pools[j].chunk = NULL;
            }
            return err;
        }
    }
    return SCL_OK;
}

scl_error_t scl_alloc_slab_alloc(scl_alloc_slab_t *slab, size_t size, void **out_ptr) {
    if (__builtin_expect(!slab, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!out_ptr, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(size == 0, 0)) return SCL_ERR_INVALID_ARG;

    size_t idx = slab_class_for_size(size);
    if (__builtin_expect(idx >= SCL_ALLOC_SLAB_CLASSES, 0))
        return SCL_ERR_INVALID_ARG;

    scl_alloc_slab_pool_t *p = &slab->pools[idx];
    if (__builtin_expect(!p->free_list, 0))
        return SCL_ERR_OUT_OF_MEMORY;

    void *block = p->free_list;
    p->free_list = *(void **)block;
    p->free_count--;
    *out_ptr = block;
    return SCL_OK;
}

scl_error_t scl_alloc_slab_free(scl_alloc_slab_t *slab, void *ptr) {
    if (__builtin_expect(!slab, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!ptr, 0)) return SCL_ERR_NULL_PTR;

    for (int i = 0; i < SCL_ALLOC_SLAB_CLASSES; i++) {
        scl_alloc_slab_pool_t *p = &slab->pools[i];
        unsigned char *start = (unsigned char *)p->chunk;
        unsigned char *end = start + p->block_size * p->total_blocks;
        if ((unsigned char *)ptr >= start && (unsigned char *)ptr < end) {
            size_t offset = (unsigned char *)ptr - start;
            if (offset % p->block_size != 0)
                return SCL_ERR_INVALID_ARG;
            if (p->free_count >= p->total_blocks)
                return SCL_ERR_INVALID_STATE;
            *(void **)ptr = p->free_list;
            p->free_list = ptr;
            p->free_count++;
            return SCL_OK;
        }
    }
    return SCL_ERR_INVALID_ARG;
}

scl_error_t scl_alloc_slab_destroy(scl_alloc_slab_t *slab) {
    if (__builtin_expect(!slab, 0)) return SCL_ERR_NULL_PTR;
    for (int i = 0; i < SCL_ALLOC_SLAB_CLASSES; i++) {
        free(slab->pools[i].chunk);
        slab->pools[i].chunk = NULL;
        slab->pools[i].free_list = NULL;
    }
    return SCL_OK;
}
