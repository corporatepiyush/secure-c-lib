#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_rand_prng.h"
#include "scl_stdlib.h"
#include "scl_string.h"

static inline uint64_t rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

static uint64_t splitmix64_next(uint64_t *state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

scl_error_t scl_rand_prng_init(scl_rand_prng_t *rng, uint64_t seed) {
    if (!rng) return SCL_ERR_NULL_PTR;
    uint64_t sm = seed;
    for (int i = 0; i < 4; i++)
        rng->state[i] = splitmix64_next(&sm);
    return SCL_OK;
}

scl_error_t scl_rand_prng_init_array(scl_rand_prng_t *rng, const uint64_t state[4]) {
    if (!rng || !state) return SCL_ERR_NULL_PTR;
    scl_memcpy(rng->state, state, sizeof(rng->state));
    return SCL_OK;
}

uint64_t scl_rand_prng_next(scl_rand_prng_t *rng) {
    if (!rng) return 0;
    const uint64_t result = rotl(rng->state[1] * 5, 7) * 9;
    const uint64_t t = rng->state[1] << 17;
    rng->state[2] ^= rng->state[0];
    rng->state[3] ^= rng->state[1];
    rng->state[1] ^= rng->state[2];
    rng->state[0] ^= rng->state[3];
    rng->state[2] ^= t;
    rng->state[3] = rotl(rng->state[3], 45);
    return result;
}

double scl_rand_prng_next_double(scl_rand_prng_t *rng) {
    if (!rng) return 0.0;
    return (scl_rand_prng_next(rng) >> 11) * 0x1.0p-53;
}

uint64_t scl_rand_prng_next_bounded(scl_rand_prng_t *rng, uint64_t bound) {
    if (!rng || bound == 0) return 0;
    uint64_t threshold = -bound % bound;
    uint64_t val;
    do {
        val = scl_rand_prng_next(rng);
    } while (val < threshold);
    return val % bound;
}

void scl_rand_prng_jump(scl_rand_prng_t *rng) {
    if (!rng) return;
    static const uint64_t JUMP[] = {
        0x180ec6d33cfd0abaULL,
        0xd5a61266f0c9392cULL,
        0xa9582618e03fc9aaULL,
        0x39abdc4539b888b8ULL
    };
    uint64_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    for (int i = 0; i < 4; i++)
        for (int b = 0; b < 64; b++) {
            if (JUMP[i] & (1ULL << b)) {
                s0 ^= rng->state[0];
                s1 ^= rng->state[1];
                s2 ^= rng->state[2];
                s3 ^= rng->state[3];
            }
            scl_rand_prng_next(rng);
        }
    rng->state[0] = s0;
    rng->state[1] = s1;
    rng->state[2] = s2;
    rng->state[3] = s3;
}

void scl_rand_prng_get_state(const scl_rand_prng_t *rng, uint64_t out_state[4]) {
    if (!rng || !out_state) return;
    scl_memcpy(out_state, rng->state, sizeof(rng->state));
}

void scl_rand_prng_fill(scl_rand_prng_t *rng, void *buf, size_t len) {
    if (!rng || !buf) return;
    unsigned char *p = (unsigned char *)buf;
    size_t i = 0;
    while (i + 8 <= len) {
        uint64_t val = scl_rand_prng_next(rng);
        scl_memcpy(p + i, &val, 8);
        i += 8;
    }
    if (i < len) {
        uint64_t val = scl_rand_prng_next(rng);
        scl_memcpy(p + i, &val, len - i);
    }
}
