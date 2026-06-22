#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "scl_rand_distribution.h"

static int failed = 0;

#define TEST(name) do { printf("  " name "... "); } while(0)
#define PASS() do { printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)

static void test_uniform_range(void) {
    TEST("uniform values in [min, max]");
    scl_rand_dist_t dist;
    (void)scl_rand_dist_init(&dist, 42);
    double min = 5.0, max = 10.0;
    for (int i = 0; i < 1000; i++) {
        double v = scl_rand_dist_uniform(&dist, min, max);
        if (v < min || v >= max) { FAIL("out of range"); return; }
    }
    PASS();
}

static void test_normal(void) {
    TEST("normal approx mean/stddev");
    scl_rand_dist_t dist;
    (void)scl_rand_dist_init(&dist, 123);
    double mean = 0.0, stddev = 1.0;
    double sum = 0.0, sum_sq = 0.0;
    int n = 10000;
    for (int i = 0; i < n; i++) {
        double v = scl_rand_dist_normal(&dist, mean, stddev);
        sum += v;
        sum_sq += v * v;
    }
    double avg = sum / n;
    double var = sum_sq / n - avg * avg;
    if (fabs(avg - mean) > 0.15) { FAIL("mean too far off"); return; }
    if (fabs(var - 1.0) > 0.15) { FAIL("variance too far off"); return; }
    PASS();
}

static void test_bernoulli(void) {
    TEST("bernoulli approx p proportion");
    scl_rand_dist_t dist;
    (void)scl_rand_dist_init(&dist, 77);
    double p = 0.3;
    int count = 0, trials = 10000;
    for (int i = 0; i < trials; i++)
        if (scl_rand_dist_bernoulli(&dist, p)) count++;
    double prop = (double)count / trials;
    if (fabs(prop - p) > 0.04) { FAIL("proportion too far off"); return; }
    PASS();
}

static void test_exponential(void) {
    TEST("exponential positive values");
    scl_rand_dist_t dist;
    (void)scl_rand_dist_init(&dist, 55);
    double lambda = 2.0;
    double sum = 0.0;
    int n = 5000;
    for (int i = 0; i < n; i++) {
        double v = scl_rand_dist_exponential(&dist, lambda);
        if (v < 0.0) { FAIL("negative value"); return; }
        sum += v;
    }
    double avg = sum / n;
    if (fabs(avg - 1.0 / lambda) > 0.1) { FAIL("mean too far off"); return; }
    PASS();
}

static void test_shuffle(void) {
    TEST("shuffle produces permutation");
    scl_rand_dist_t dist;
    (void)scl_rand_dist_init(&dist, 222);
    int arr[100];
    for (int i = 0; i < 100; i++) arr[i] = i;
    if (scl_rand_dist_shuffle(&dist, arr, 100, sizeof(int)) != SCL_OK)
        { FAIL("shuffle returned error"); return; }
    int sum = 0;
    for (int i = 0; i < 100; i++) sum += arr[i];
    if (sum != 99 * 100 / 2) { FAIL("not a permutation"); return; }
    PASS();
}

static void test_sample(void) {
    TEST("sample produces valid subset");
    scl_rand_dist_t dist;
    (void)scl_rand_dist_init(&dist, 333);
    size_t n = 50, k = 10;
    size_t *indices = (size_t *)malloc(k * sizeof(size_t));
    if (!indices) { FAIL("malloc"); return; }
    if (scl_rand_dist_sample(&dist, n, k, indices) != SCL_OK)
        { FAIL("sample returned error"); free(indices); return; }
    for (size_t i = 0; i < k; i++) {
        if (indices[i] >= n) { FAIL("index out of range"); free(indices); return; }
        for (size_t j = i + 1; j < k; j++)
            if (indices[i] == indices[j]) { FAIL("duplicate index"); free(indices); return; }
    }
    free(indices);
    PASS();
}

int main(void) {
    printf("scl_rand_distribution tests:\n");
    test_uniform_range();
    test_normal();
    test_bernoulli();
    test_exponential();
    test_shuffle();
    test_sample();
    printf("\n%d test(s) failed\n", failed);
    return failed ? 1 : 0;
}
