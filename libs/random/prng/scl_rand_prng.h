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

/* Xoshiro256** PRNG (256-bit state, 64-bit output, passes BigCrush). Splitmix64
 * seeding. Fastest non-crypto PRNG. */

#ifndef SCL_RAND_PRNG_H
#define SCL_RAND_PRNG_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

typedef struct {
  uint64_t state[4];
} scl_rand_prng_t;

scl_error_t scl_rand_prng_init(scl_rand_prng_t *rng,
                               uint64_t seed) SCL_WARN_UNUSED;
scl_error_t scl_rand_prng_init_array(scl_rand_prng_t *rng,
                                     const uint64_t *state) SCL_WARN_UNUSED;
uint64_t scl_rand_prng_next(scl_rand_prng_t *rng);
double scl_rand_prng_next_double(scl_rand_prng_t *rng);
uint64_t scl_rand_prng_next_bounded(scl_rand_prng_t *rng, uint64_t bound);
void scl_rand_prng_jump(scl_rand_prng_t *rng);
void scl_rand_prng_get_state(const scl_rand_prng_t *rng, uint64_t *out_state);
void scl_rand_prng_fill(scl_rand_prng_t *rng, void *buf, size_t len);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
