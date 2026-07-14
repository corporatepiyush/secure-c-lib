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

/* Statistical distributions: Uniform (int/real), Normal (Box-Muller),
 * Exponential, Bernoulli, Poisson, Fisher-Yates shuffle. */

#ifndef SCL_RAND_DISTRIBUTION_H
#define SCL_RAND_DISTRIBUTION_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "prng/scl_rand_prng.h"
#include "scl_common.h"

typedef struct {
  scl_rand_prng_t rng;
} scl_rand_dist_t;

scl_error_t scl_rand_dist_init(scl_rand_dist_t *dist,
                               uint64_t seed) SCL_WARN_UNUSED;
double scl_rand_dist_uniform(scl_rand_dist_t *dist, double min, double max);
double scl_rand_dist_normal(scl_rand_dist_t *dist, double mean, double stddev);
double scl_rand_dist_exponential(scl_rand_dist_t *dist, double lambda);
bool scl_rand_dist_bernoulli(scl_rand_dist_t *dist, double p);
int64_t scl_rand_dist_poisson(scl_rand_dist_t *dist, double lambda);
scl_error_t scl_rand_dist_shuffle(scl_rand_dist_t *dist, void *base,
                                  size_t count,
                                  size_t elem_size) SCL_WARN_UNUSED;
scl_error_t scl_rand_dist_sample(scl_allocator_t *alloc, scl_rand_dist_t *dist,
                                 size_t n, size_t k,
                                 size_t *out_indices) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
