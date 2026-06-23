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

scl_error_t scl_rand_noise_init(scl_rand_noise_t *noise, uint64_t seed) SCL_WARN_UNUSED;
double scl_rand_noise_value1d(scl_rand_noise_t *noise, double x);
double scl_rand_noise_value2d(scl_rand_noise_t *noise, double x, double y);
double scl_rand_noise_perlin1d(scl_rand_noise_t *noise, double x);
double scl_rand_noise_perlin2d(scl_rand_noise_t *noise, double x, double y);
double scl_rand_noise_perlin3d(scl_rand_noise_t *noise, double x, double y, double z);
double scl_rand_noise_white(scl_rand_noise_t *noise, double x, double y);
double scl_rand_noise_fbm(scl_rand_noise_t *noise, double x, double y, int octaves, double lacunarity, double gain);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
