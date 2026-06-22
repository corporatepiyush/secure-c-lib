#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_alloc_pool.h"
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>

static inline size_t scl_align_up(size_t size, size_t align) {
    size_t mod = size % align;
    return mod ? size + (align - mod) : size;
}

scl_error_t scl_alloc_pool_init(scl_alloc_pool_t *pool, size_t block_size, size_t block_count) {
    if (__builtin_expect(!pool, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(block_size == 0 || block_count == 0, 0))
        return SCL_ERR_INVALID_ARG;

    size_t aligned_bs = scl_align_up(block_size, alignof(max_align_t));
    if (aligned_bs < sizeof(void *))
        aligned_bs = sizeof(void *);

    size_t total;
    if (__builtin_expect(scl_mul_overflow(aligned_bs, block_count, &total), 0))
        return SCL_ERR_SIZE_OVERFLOW;

    void *chunk = malloc(total);
    if (__builtin_expect(!chunk, 0)) return SCL_ERR_OUT_OF_MEMORY;

    unsigned char *ptr = (unsigned char *)chunk;
    void *prev = NULL;
    for (size_t i = 0; i < block_count; i++) {
        void *block = ptr + i * aligned_bs;
        *(void **)block = prev;
        prev = block;
    }

    pool->chunk = chunk;
    pool->free_list = prev;
    pool->block_size = aligned_bs;
    pool->total_blocks = block_count;
    pool->free_count = block_count;
    return SCL_OK;
}

scl_error_t scl_alloc_pool_alloc(scl_alloc_pool_t *pool, void **out_ptr) {
    if (__builtin_expect(!pool, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!out_ptr, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!pool->free_list, 0)) return SCL_ERR_OUT_OF_MEMORY;

    void *block = pool->free_list;
    pool->free_list = *(void **)block;
    pool->free_count--;
    *out_ptr = block;
    return SCL_OK;
}

scl_error_t scl_alloc_pool_free(scl_alloc_pool_t *pool, void *ptr) {
    if (__builtin_expect(!pool, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!ptr, 0)) return SCL_ERR_NULL_PTR;

    unsigned char *chunk_end = (unsigned char *)pool->chunk + pool->block_size * pool->total_blocks;
    if (__builtin_expect((unsigned char *)ptr < (unsigned char *)pool->chunk ||
                         (unsigned char *)ptr >= chunk_end, 0))
        return SCL_ERR_INVALID_ARG;

    size_t offset = (unsigned char *)ptr - (unsigned char *)pool->chunk;
    if (__builtin_expect(offset % pool->block_size != 0, 0))
        return SCL_ERR_INVALID_ARG;

    if (__builtin_expect(pool->free_count == pool->total_blocks, 0))
        return SCL_ERR_INVALID_STATE;

    *(void **)ptr = pool->free_list;
    pool->free_list = ptr;
    pool->free_count++;
    return SCL_OK;
}

scl_error_t scl_alloc_pool_destroy(scl_alloc_pool_t *pool) {
    if (__builtin_expect(!pool, 0)) return SCL_ERR_NULL_PTR;
    free(pool->chunk);
    pool->chunk = NULL;
    pool->free_list = NULL;
    pool->block_size = 0;
    pool->total_blocks = 0;
    pool->free_count = 0;
    return SCL_OK;
}
