#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "scl_rand_noise.h"

static int failed = 0;

#define TEST(name) do { printf("  " name "... "); } while(0)
#define PASS() do { printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)

static void test_deterministic(void) {
    TEST("same seed gives same noise values");
    scl_rand_noise_t a, b;
    (void)scl_rand_noise_init(&a, 42);
    (void)scl_rand_noise_init(&b, 42);
    for (int i = 0; i < 100; i++) {
        double x = (double)i * 0.1;
        double va = scl_rand_noise_perlin2d(&a, x, x + 0.5);
        double vb = scl_rand_noise_perlin2d(&b, x, x + 0.5);
        if (va != vb) { FAIL("deterministic mismatch"); return; }
    }
    PASS();
}

static void test_value1d_range(void) {
    TEST("value1d output in [-1, 1]");
    scl_rand_noise_t noise;
    (void)scl_rand_noise_init(&noise, 7);
    for (int i = 0; i < 500; i++) {
        double v = scl_rand_noise_value1d(&noise, (double)i * 0.05);
        if (v < -1.0 || v > 1.0) { FAIL("out of range"); return; }
    }
    PASS();
}

static void test_value2d_range(void) {
    TEST("value2d output in [-1, 1]");
    scl_rand_noise_t noise;
    (void)scl_rand_noise_init(&noise, 13);
    for (int i = 0; i < 500; i++) {
        double x = (double)i * 0.03;
        double y = (double)i * 0.07;
        double v = scl_rand_noise_value2d(&noise, x, y);
        if (v < -1.0 || v > 1.0) { FAIL("out of range"); return; }
    }
    PASS();
}

static void test_perlin2d_range(void) {
    TEST("perlin2d output in [-1, 1]");
    scl_rand_noise_t noise;
    (void)scl_rand_noise_init(&noise, 99);
    for (int i = 0; i < 500; i++) {
        double x = (double)i * 0.05;
        double y = (double)i * 0.03 + 1.0;
        double v = scl_rand_noise_perlin2d(&noise, x, y);
        if (v < -1.0 || v > 1.0) { FAIL("out of range"); return; }
    }
    PASS();
}

static void test_perlin3d_range(void) {
    TEST("perlin3d output in [-1, 1]");
    scl_rand_noise_t noise;
    (void)scl_rand_noise_init(&noise, 55);
    for (int i = 0; i < 300; i++) {
        double x = (double)i * 0.07;
        double y = (double)i * 0.11;
        double z = (double)i * 0.13;
        double v = scl_rand_noise_perlin3d(&noise, x, y, z);
        if (v < -1.0 || v > 1.0) { FAIL("out of range"); return; }
    }
    PASS();
}

static void test_continuity(void) {
    TEST("close points give similar values");
    scl_rand_noise_t noise;
    (void)scl_rand_noise_init(&noise, 1);
    double v1 = scl_rand_noise_perlin2d(&noise, 0.5, 0.5);
    double v2 = scl_rand_noise_perlin2d(&noise, 0.5001, 0.5001);
    double diff = fabs(v1 - v2);
    if (diff > 0.01) { FAIL("not continuous"); return; }
    PASS();
}

static void test_fbm(void) {
    TEST("fbm produces different output");
    scl_rand_noise_t noise;
    (void)scl_rand_noise_init(&noise, 777);
    double v = scl_rand_noise_fbm(&noise, 1.0, 2.0, 6, 2.0, 0.5);
    if (v < -1.0 || v > 1.0) { FAIL("fbm out of range"); return; }
    PASS();
}

int main(void) {
    printf("scl_rand_noise tests:\n");
    test_deterministic();
    test_value1d_range();
    test_value2d_range();
    test_perlin2d_range();
    test_perlin3d_range();
    test_continuity();
    test_fbm();
    printf("\n%d test(s) failed\n", failed);
    return failed ? 1 : 0;
}
