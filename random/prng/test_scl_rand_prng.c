#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "scl_rand_prng.h"

static int failed = 0;

#define TEST(name) do { printf("  " name "... "); } while(0)
#define PASS() do { printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)

static void test_init_and_next(void) {
    TEST("init and next produces non-uniform values");
    scl_rand_prng_t rng;
    if (scl_rand_prng_init(&rng, 42) != SCL_OK) { FAIL("init"); return; }
    uint64_t first = scl_rand_prng_next(&rng);
    int all_same = 1;
    for (int i = 0; i < 10000; i++) {
        uint64_t v = scl_rand_prng_next(&rng);
        if (v != first) all_same = 0;
        if (i == 0 && v == first) all_same = 1;
    }
    if (!all_same) PASS();
    else FAIL("all values are identical");
}

static void test_deterministic(void) {
    TEST("same seed gives same sequence");
    scl_rand_prng_t a, b;
    (void)scl_rand_prng_init(&a, 12345);
    (void)scl_rand_prng_init(&b, 12345);
    for (int i = 0; i < 1000; i++) {
        uint64_t va = scl_rand_prng_next(&a);
        uint64_t vb = scl_rand_prng_next(&b);
        if (va != vb) { FAIL("mismatch"); return; }
    }
    PASS();
}

static void test_bounded(void) {
    TEST("bounded values in range [0, bound)");
    scl_rand_prng_t rng;
    (void)scl_rand_prng_init(&rng, 99);
    uint64_t bound = 100;
    for (int i = 0; i < 10000; i++) {
        uint64_t v = scl_rand_prng_next_bounded(&rng, bound);
        if (v >= bound) { FAIL("out of range"); return; }
    }
    PASS();
}

static void test_jump(void) {
    TEST("jump produces different sequence");
    scl_rand_prng_t a, b;
    (void)scl_rand_prng_init(&a, 7);
    (void)scl_rand_prng_init(&b, 7);
    scl_rand_prng_jump(&b);
    uint64_t va = scl_rand_prng_next(&a);
    uint64_t vb = scl_rand_prng_next(&b);
    if (va == vb) { FAIL("jump did not change state"); return; }
    PASS();
}

static void test_fill(void) {
    TEST("fill produces non-zero bytes");
    scl_rand_prng_t rng;
    (void)scl_rand_prng_init(&rng, 1);
    unsigned char buf[64];
    memset(buf, 0, sizeof(buf));
    scl_rand_prng_fill(&rng, buf, sizeof(buf));
    int all_zero = 1;
    for (size_t i = 0; i < sizeof(buf); i++)
        if (buf[i]) { all_zero = 0; break; }
    if (all_zero) { FAIL("all bytes zero"); return; }
    PASS();
}

static void test_get_state(void) {
    TEST("get_state returns correct state");
    scl_rand_prng_t rng;
    (void)scl_rand_prng_init(&rng, 555);
    uint64_t st[4];
    scl_rand_prng_get_state(&rng, st);
    for (int i = 0; i < 4; i++)
        if (st[i] != rng.state[i]) { FAIL("state mismatch"); return; }
    PASS();
}

static void test_init_array(void) {
    TEST("init_array works correctly");
    uint64_t st[4] = {1, 2, 3, 4};
    scl_rand_prng_t rng;
    if (scl_rand_prng_init_array(&rng, st) != SCL_OK) { FAIL("init_array"); return; }
    uint64_t out[4];
    scl_rand_prng_get_state(&rng, out);
    for (int i = 0; i < 4; i++)
        if (out[i] != st[i]) { FAIL("state mismatch"); return; }
    PASS();
}

int main(void) {
    printf("scl_rand_prng tests:\n");
    test_init_and_next();
    test_deterministic();
    test_bounded();
    test_jump();
    test_fill();
    test_get_state();
    test_init_array();
    printf("\n%d test(s) failed\n", failed);
    return failed ? 1 : 0;
}
