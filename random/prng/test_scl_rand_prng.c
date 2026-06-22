#include "../../testlib/scl_test.h"
#include "scl_rand_prng.h"
#include "../../stdlib/scl_stdlib.h"

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    scl_test_group("init_and_next");
    {
        scl_rand_prng_t rng;
        SCL_EXPECT_OK(&tr, scl_rand_prng_init(&rng, 42));
        uint64_t first = scl_rand_prng_next(&rng);
        int all_same = 1;
        for (int i = 0; i < 10000; i++) {
            uint64_t v = scl_rand_prng_next(&rng);
            if (v != first) all_same = 0;
        }
        SCL_EXPECT_FALSE(&tr, all_same);
    }

    scl_test_group("deterministic");
    {
        scl_rand_prng_t a, b;
        (void)scl_rand_prng_init(&a, 12345);
        (void)scl_rand_prng_init(&b, 12345);
        int ok = 1;
        for (int i = 0; i < 1000; i++) {
            if (scl_rand_prng_next(&a) != scl_rand_prng_next(&b)) { ok = 0; break; }
        }
        SCL_EXPECT_TRUE(&tr, ok);
    }

    scl_test_group("bounded");
    {
        scl_rand_prng_t rng;
        (void)scl_rand_prng_init(&rng, 99);
        uint64_t bound = 100;
        int ok = 1;
        for (int i = 0; i < 10000; i++) {
            if (scl_rand_prng_next_bounded(&rng, bound) >= bound) { ok = 0; break; }
        }
        SCL_EXPECT_TRUE(&tr, ok);
    }

    scl_test_group("jump");
    {
        scl_rand_prng_t a, b;
        (void)scl_rand_prng_init(&a, 7);
        (void)scl_rand_prng_init(&b, 7);
        scl_rand_prng_jump(&b);
        uint64_t va = scl_rand_prng_next(&a);
        uint64_t vb = scl_rand_prng_next(&b);
        SCL_EXPECT_FALSE(&tr, va == vb);
    }

    scl_test_group("fill");
    {
        scl_rand_prng_t rng;
        (void)scl_rand_prng_init(&rng, 1);
        unsigned char buf[64];
        scl_memset(buf, 0, sizeof(buf));
        scl_rand_prng_fill(&rng, buf, sizeof(buf));
        int all_zero = 1;
        for (size_t i = 0; i < sizeof(buf); i++)
            if (buf[i]) { all_zero = 0; break; }
        SCL_EXPECT_FALSE(&tr, all_zero);
    }

    scl_test_group("get_state");
    {
        scl_rand_prng_t rng;
        (void)scl_rand_prng_init(&rng, 555);
        uint64_t st[4];
        scl_rand_prng_get_state(&rng, st);
        int ok = 1;
        for (int i = 0; i < 4; i++)
            if (st[i] != rng.state[i]) { ok = 0; break; }
        SCL_EXPECT_TRUE(&tr, ok);
    }

    scl_test_group("init_array");
    {
        uint64_t st[4] = {1, 2, 3, 4};
        scl_rand_prng_t rng;
        SCL_EXPECT_OK(&tr, scl_rand_prng_init_array(&rng, st));
        uint64_t out[4];
        scl_rand_prng_get_state(&rng, out);
        int ok = 1;
        for (int i = 0; i < 4; i++)
            if (out[i] != st[i]) { ok = 0; break; }
        SCL_EXPECT_TRUE(&tr, ok);
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
