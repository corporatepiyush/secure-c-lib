#ifndef SCL_RAND_PRNG_H
#define SCL_RAND_PRNG_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../../common/scl_common.h"
#include <string.h>

typedef struct {
    uint64_t state[4];
} scl_rand_prng_t;

scl_error_t scl_rand_prng_init(scl_rand_prng_t *rng, uint64_t seed) SCL_WARN_UNUSED;
scl_error_t scl_rand_prng_init_array(scl_rand_prng_t *rng, const uint64_t state[4]) SCL_WARN_UNUSED;
uint64_t scl_rand_prng_next(scl_rand_prng_t *rng);
double scl_rand_prng_next_double(scl_rand_prng_t *rng);
uint64_t scl_rand_prng_next_bounded(scl_rand_prng_t *rng, uint64_t bound);
void scl_rand_prng_jump(scl_rand_prng_t *rng);
void scl_rand_prng_get_state(const scl_rand_prng_t *rng, uint64_t out_state[4]);
void scl_rand_prng_fill(scl_rand_prng_t *rng, void *buf, size_t len);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
