#ifndef SCL_ALLOC_TLSF_H
#define SCL_ALLOC_TLSF_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../../common/scl_common.h"
#include <stddef.h>

#define SCL_TLSF_FL_MAX    32
#define SCL_TLSF_SL_COUNT  32
#define SCL_TLSF_SL_LOG2   5
#define SCL_TLSF_SMALL_BLOCK 256

typedef struct scl_tlsf_block_hdr {
    struct scl_tlsf_block_hdr *prev_phys;
    size_t size;
    struct scl_tlsf_block_hdr *next_free;
    struct scl_tlsf_block_hdr *prev_free;
} scl_tlsf_block_hdr_t;

typedef struct {
    void *pool;
    size_t pool_size;
    unsigned int fl_bitmap;
    unsigned int sl_bitmap[SCL_TLSF_FL_MAX];
    scl_tlsf_block_hdr_t *bins[SCL_TLSF_FL_MAX][SCL_TLSF_SL_COUNT];
    scl_tlsf_block_hdr_t *block_sentinel;
} scl_alloc_tlsf_t;

scl_error_t scl_alloc_tlsf_init(scl_alloc_tlsf_t *tlsf, size_t pool_size);
scl_error_t scl_alloc_tlsf_alloc(scl_alloc_tlsf_t *tlsf, size_t size, void **out_ptr);
scl_error_t scl_alloc_tlsf_free(scl_alloc_tlsf_t *tlsf, void *ptr);
scl_error_t scl_alloc_tlsf_destroy(scl_alloc_tlsf_t *tlsf);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
