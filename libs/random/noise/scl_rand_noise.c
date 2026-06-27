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

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_rand_noise.h"
#include "scl_math.h"
#include "scl_stdlib.h"
#include "scl_string.h"

static SCL_ALWAYS_INLINE SCL_PURE double fade(double t) {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

static SCL_ALWAYS_INLINE SCL_PURE double lerp(double a, double b, double t) {
    return a + t * (b - a);
}

static SCL_ALWAYS_INLINE SCL_PURE double smoothstep(double t) {
    return t * t * (3.0 - 2.0 * t);
}

static SCL_ALWAYS_INLINE SCL_PURE double grad1d(int hash, double x) {
    return (hash & 1) ? x : -x;
}

static SCL_ALWAYS_INLINE SCL_PURE double grad2d(int hash, double x, double y) {
    int h = hash & 3;
    double u = (h < 2) ? x : y;
    double v = (h < 2) ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

static SCL_ALWAYS_INLINE SCL_PURE double grad3d(int hash, double x, double y, double z) {
    int h = hash & 15;
    double u = (h < 8) ? x : y;
    double v = (h < 4) ? y : ((h == 12 || h == 14) ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

static uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

scl_error_t scl_rand_noise_init(scl_rand_noise_t * noise, uint64_t seed) {
    if (scl_unlikely(!noise)) return SCL_ERR_NULL_PTR;
    noise->seed = seed;
    noise->freq = 1.0;
    int arr[256];
    for (int i = 0; i < 256; i++) arr[i] = i;
    uint64_t sm = seed;
    for (int i = 255; i > 0; i--) {
        uint64_t r = splitmix64(&sm);
        int j = (int)(r % (uint64_t)(i + 1));
        int tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
    }
    for (int i = 0; i < 512; i++)
        noise->perm[i] = arr[i & 255];
    return SCL_OK;
}

double scl_rand_noise_value1d(scl_rand_noise_t * noise, double x) {
    if (scl_unlikely(!noise)) return 0.0;
    double fx = x * noise->freq;
    int ix = (int)scl_floor(fx) & 255;
    double frac = fx - scl_floor(fx);
    double s = smoothstep(frac);
    int a = noise->perm[ix];
    int b = noise->perm[ix + 1];
    double va = (double)a / 255.0 * 2.0 - 1.0;
    double vb = (double)b / 255.0 * 2.0 - 1.0;
    return lerp(va, vb, s);
}

double scl_rand_noise_value2d(scl_rand_noise_t * noise, double x, double y) {
    if (scl_unlikely(!noise)) return 0.0;
    double fx = x * noise->freq, fy = y * noise->freq;
    int ix = (int)scl_floor(fx) & 255;
    int iy = (int)scl_floor(fy) & 255;
    double fracx = fx - scl_floor(fx);
    double fracy = fy - scl_floor(fy);
    double sx = smoothstep(fracx);
    double sy = smoothstep(fracy);
    int aa = noise->perm[noise->perm[ix] + iy];
    int ab = noise->perm[noise->perm[ix] + iy + 1];
    int ba = noise->perm[noise->perm[ix + 1] + iy];
    int bb = noise->perm[noise->perm[ix + 1] + iy + 1];
    double vaa = (double)aa / 255.0 * 2.0 - 1.0;
    double vab = (double)ab / 255.0 * 2.0 - 1.0;
    double vba = (double)ba / 255.0 * 2.0 - 1.0;
    double vbb = (double)bb / 255.0 * 2.0 - 1.0;
    double l1 = lerp(vaa, vba, sx);
    double l2 = lerp(vab, vbb, sx);
    return lerp(l1, l2, sy);
}

double scl_rand_noise_perlin1d(scl_rand_noise_t * noise, double x) {
    if (scl_unlikely(!noise)) return 0.0;
    double fx = x * noise->freq;
    int ix = (int)scl_floor(fx);
    double frac = fx - scl_floor(fx);
    double f = fade(frac);
    double n0 = grad1d(noise->perm[ix & 255], frac);
    double n1 = grad1d(noise->perm[(ix + 1) & 255], frac - 1.0);
    return lerp(n0, n1, f);
}

double scl_rand_noise_perlin2d(scl_rand_noise_t * noise, double x, double y) {
    if (scl_unlikely(!noise)) return 0.0;
    double fx = x * noise->freq, fy = y * noise->freq;
    int ix = (int)scl_floor(fx);
    int iy = (int)scl_floor(fy);
    double fracx = fx - scl_floor(fx);
    double fracy = fy - scl_floor(fy);
    double u = fade(fracx);
    double v = fade(fracy);
    int aa = noise->perm[noise->perm[ix & 255] + (iy & 255)];
    int ab = noise->perm[noise->perm[ix & 255] + ((iy + 1) & 255)];
    int ba = noise->perm[noise->perm[(ix + 1) & 255] + (iy & 255)];
    int bb = noise->perm[noise->perm[(ix + 1) & 255] + ((iy + 1) & 255)];
    double x1 = lerp(grad2d(aa, fracx, fracy), grad2d(ba, fracx - 1.0, fracy), u);
    double x2 = lerp(grad2d(ab, fracx, fracy - 1.0), grad2d(bb, fracx - 1.0, fracy - 1.0), u);
    return lerp(x1, x2, v);
}

double scl_rand_noise_perlin3d(scl_rand_noise_t * noise, double x, double y, double z) {
    if (scl_unlikely(!noise)) return 0.0;
    double fx = x * noise->freq, fy = y * noise->freq, fz = z * noise->freq;
    int ix = (int)scl_floor(fx);
    int iy = (int)scl_floor(fy);
    int iz = (int)scl_floor(fz);
    double fracx = fx - scl_floor(fx);
    double fracy = fy - scl_floor(fy);
    double fracz = fz - scl_floor(fz);
    double u = fade(fracx);
    double v = fade(fracy);
    double w = fade(fracz);
    int p[512];
    scl_memcpy(p, noise->perm, sizeof(p));
    int aaa = p[p[p[ix & 255] + (iy & 255)] + (iz & 255)];
    int aba = p[p[p[ix & 255] + ((iy + 1) & 255)] + (iz & 255)];
    int aab = p[p[p[ix & 255] + (iy & 255)] + ((iz + 1) & 255)];
    int abb = p[p[p[ix & 255] + ((iy + 1) & 255)] + ((iz + 1) & 255)];
    int baa = p[p[p[(ix + 1) & 255] + (iy & 255)] + (iz & 255)];
    int bba = p[p[p[(ix + 1) & 255] + ((iy + 1) & 255)] + (iz & 255)];
    int bab = p[p[p[(ix + 1) & 255] + (iy & 255)] + ((iz + 1) & 255)];
    int bbb = p[p[p[(ix + 1) & 255] + ((iy + 1) & 255)] + ((iz + 1) & 255)];
    double x11 = lerp(grad3d(aaa, fracx, fracy, fracz), grad3d(baa, fracx - 1.0, fracy, fracz), u);
    double x12 = lerp(grad3d(aba, fracx, fracy - 1.0, fracz), grad3d(bba, fracx - 1.0, fracy - 1.0, fracz), u);
    double x21 = lerp(grad3d(aab, fracx, fracy, fracz - 1.0), grad3d(bab, fracx - 1.0, fracy, fracz - 1.0), u);
    double x22 = lerp(grad3d(abb, fracx, fracy - 1.0, fracz - 1.0), grad3d(bbb, fracx - 1.0, fracy - 1.0, fracz - 1.0), u);
    double y1 = lerp(x11, x12, v);
    double y2 = lerp(x21, x22, v);
    return lerp(y1, y2, w);
}

double scl_rand_noise_white(scl_rand_noise_t * noise, double x, double y) {
    if (scl_unlikely(!noise)) return 0.0;
    (void)x;
    (void)y;
    uint64_t sm = noise->seed;
    uint64_t r = splitmix64(&sm);
    return (double)(r & 0x1FFFFFFFFFFFFFULL) / 9007199254740991.0 * 2.0 - 1.0;
}

double scl_rand_noise_fbm(scl_rand_noise_t * noise, double x, double y, int octaves, double lacunarity, double gain) {
    if (scl_unlikely(!noise || octaves < 1)) return 0.0;
    double total = 0.0;
    double amplitude = 1.0;
    double max_amplitude = 0.0;
    double px = x, py = y;
    scl_rand_noise_t n = *noise;
    for (int i = 0; i < octaves; i++) {
        total += amplitude * scl_rand_noise_perlin2d(&n, px, py);
        max_amplitude += amplitude;
        px *= lacunarity;
        py *= lacunarity;
        amplitude *= gain;
    }
    return total / max_amplitude;
}
