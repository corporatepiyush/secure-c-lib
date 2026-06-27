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

/* Statistical distributions: Uniform (int/real), Normal (Box-Muller), Exponential, Bernoulli, Poisson, Fisher-Yates shuffle. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_rand_distribution.h"
#include "scl_math.h"
#include "scl_stdlib.h"
#include "scl_string.h"

scl_error_t scl_rand_dist_init(scl_rand_dist_t * dist, uint64_t seed) {
    if (scl_unlikely(!dist)) return SCL_ERR_NULL_PTR;
    return scl_rand_prng_init(&dist->rng, seed);
}

double scl_rand_dist_uniform(scl_rand_dist_t * dist, double min, double max) {
    if (scl_unlikely(!dist || min >= max)) return 0.0;
    return min + (max - min) * scl_rand_prng_next_double(&dist->rng);
}

double scl_rand_dist_normal(scl_rand_dist_t * dist, double mean, double stddev) {
    if (scl_unlikely(!dist || stddev < 0.0)) return 0.0;
    double u1 = scl_rand_prng_next_double(&dist->rng);
    double u2 = scl_rand_prng_next_double(&dist->rng);
    while (u1 <= 0.0) u1 = scl_rand_prng_next_double(&dist->rng);
    double z0 = scl_sqrt(-2.0 * scl_log(u1)) * cos(2.0 * M_PI * u2);
    return mean + z0 * stddev;
}

double scl_rand_dist_exponential(scl_rand_dist_t * dist, double lambda) {
    if (scl_unlikely(!dist || lambda <= 0.0)) return 0.0;
    double u = scl_rand_prng_next_double(&dist->rng);
    while (u <= 0.0) u = scl_rand_prng_next_double(&dist->rng);
    return -scl_log(u) / lambda;
}

bool scl_rand_dist_bernoulli(scl_rand_dist_t * dist, double p) {
    if (scl_unlikely(!dist)) return false;
    if (p <= 0.0) return false;
    if (p >= 1.0) return true;
    return scl_rand_prng_next_double(&dist->rng) < p;
}

int64_t scl_rand_dist_poisson(scl_rand_dist_t * dist, double lambda) {
    if (scl_unlikely(!dist || lambda <= 0.0)) return 0;
    double L = scl_exp(-lambda);
    double p = 1.0;
    int64_t k = 0;
    do {
        k++;
        double u = scl_rand_prng_next_double(&dist->rng);
        while (u <= 0.0) u = scl_rand_prng_next_double(&dist->rng);
        p *= u;
    } while (p > L);
    return k - 1;
}

static SCL_ALWAYS_INLINE void swap_elements(void * a, void * b, size_t elem_size) {
    unsigned char tmp[64];
    size_t offset = 0;
    while (offset < elem_size) {
        size_t chunk = (elem_size - offset < sizeof(tmp)) ? (elem_size - offset) : sizeof(tmp);
        scl_memcpy(tmp, (unsigned char *)a + offset, chunk);
        scl_memcpy((unsigned char *)a + offset, (unsigned char *)b + offset, chunk);
        scl_memcpy((unsigned char *)b + offset, tmp, chunk);
        offset += chunk;
    }
}

scl_error_t scl_rand_dist_shuffle(scl_rand_dist_t * dist, void * base, size_t count, size_t elem_size) {
    if (scl_unlikely(!dist || !base)) return SCL_ERR_NULL_PTR;
    if (count <= 1 || elem_size == 0) return SCL_OK;
    for (size_t i = count - 1; i > 0; i--) {
        size_t j = scl_rand_prng_next_bounded(&dist->rng, i + 1);
        if (j != i)
            swap_elements((unsigned char *)base + i * elem_size,
                          (unsigned char *)base + j * elem_size, elem_size);
    }
    return SCL_OK;
}

scl_error_t scl_rand_dist_sample(scl_allocator_t *alloc, scl_rand_dist_t * dist, size_t n, size_t k, size_t * out_indices) {
    if (scl_unlikely(!dist || !out_indices)) return SCL_ERR_NULL_PTR;
    if (k > n) return SCL_ERR_INVALID_ARG;
    if (k == 0) return SCL_OK;
    size_t *selected = (size_t *)scl_calloc(alloc, n, sizeof(size_t), _Alignof(max_align_t));
    if (scl_unlikely(!selected)) return SCL_ERR_OUT_OF_MEMORY;
    size_t filled = 0;
    for (size_t i = 0; i < n && filled < k; i++) {
        double r = (double)(n - i);
        double p = (double)(k - filled) / r;
        if (scl_rand_prng_next_double(&dist->rng) < p)
            out_indices[filled++] = i;
    }
    scl_free(alloc, selected);
    return SCL_OK;
}
