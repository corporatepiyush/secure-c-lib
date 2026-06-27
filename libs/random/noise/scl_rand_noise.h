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

/* Procedural noise: value/Perlin (1D/2D/3D), white noise, FBM. Permutation table from PRNG. Multiple octaves with lacunarity scaling. */

#ifndef SCL_RAND_NOISE_H
#define SCL_RAND_NOISE_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

typedef struct {
    int perm[512];
    double freq;
    uint64_t seed;
} scl_rand_noise_t;

scl_error_t scl_rand_noise_init(scl_rand_noise_t * noise, uint64_t seed) SCL_WARN_UNUSED;
double scl_rand_noise_value1d(scl_rand_noise_t * noise, double x);
double scl_rand_noise_value2d(scl_rand_noise_t * noise, double x, double y);
double scl_rand_noise_perlin1d(scl_rand_noise_t * noise, double x);
double scl_rand_noise_perlin2d(scl_rand_noise_t * noise, double x, double y);
double scl_rand_noise_perlin3d(scl_rand_noise_t * noise, double x, double y, double z);
double scl_rand_noise_white(scl_rand_noise_t * noise, double x, double y);
double scl_rand_noise_fbm(scl_rand_noise_t * noise, double x, double y, int octaves, double lacunarity, double gain);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
