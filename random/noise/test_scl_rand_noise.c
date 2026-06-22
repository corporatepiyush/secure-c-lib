#include "../../testlib/scl_test.h"
#include <math.h>
#include "scl_rand_noise.h"

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    scl_test_group("deterministic");
    {
        scl_rand_noise_t a, b;
        (void)scl_rand_noise_init(&a, 42);
        (void)scl_rand_noise_init(&b, 42);
        int ok = 1;
        for (int i = 0; i < 100; i++) {
            double x = (double)i * 0.1;
            double va = scl_rand_noise_perlin2d(&a, x, x + 0.5);
            double vb = scl_rand_noise_perlin2d(&b, x, x + 0.5);
            if (va != vb) { ok = 0; break; }
        }
        SCL_EXPECT_TRUE(&tr, ok);
    }

    scl_test_group("value1d_range");
    {
        scl_rand_noise_t noise;
        (void)scl_rand_noise_init(&noise, 7);
        int ok = 1;
        for (int i = 0; i < 500; i++) {
            double v = scl_rand_noise_value1d(&noise, (double)i * 0.05);
            if (v < -1.0 || v > 1.0) { ok = 0; break; }
        }
        SCL_EXPECT_TRUE(&tr, ok);
    }

    scl_test_group("value2d_range");
    {
        scl_rand_noise_t noise;
        (void)scl_rand_noise_init(&noise, 13);
        int ok = 1;
        for (int i = 0; i < 500; i++) {
            double v = scl_rand_noise_value2d(&noise, (double)i * 0.03, (double)i * 0.07);
            if (v < -1.0 || v > 1.0) { ok = 0; break; }
        }
        SCL_EXPECT_TRUE(&tr, ok);
    }

    scl_test_group("perlin2d_range");
    {
        scl_rand_noise_t noise;
        (void)scl_rand_noise_init(&noise, 99);
        int ok = 1;
        for (int i = 0; i < 500; i++) {
            double v = scl_rand_noise_perlin2d(&noise, (double)i * 0.05, (double)i * 0.03 + 1.0);
            if (v < -1.0 || v > 1.0) { ok = 0; break; }
        }
        SCL_EXPECT_TRUE(&tr, ok);
    }

    scl_test_group("perlin3d_range");
    {
        scl_rand_noise_t noise;
        (void)scl_rand_noise_init(&noise, 55);
        int ok = 1;
        for (int i = 0; i < 300; i++) {
            double v = scl_rand_noise_perlin3d(&noise, (double)i * 0.07, (double)i * 0.11, (double)i * 0.13);
            if (v < -1.0 || v > 1.0) { ok = 0; break; }
        }
        SCL_EXPECT_TRUE(&tr, ok);
    }

    scl_test_group("continuity");
    {
        scl_rand_noise_t noise;
        (void)scl_rand_noise_init(&noise, 1);
        double v1 = scl_rand_noise_perlin2d(&noise, 0.5, 0.5);
        double v2 = scl_rand_noise_perlin2d(&noise, 0.5001, 0.5001);
        double diff = fabs(v1 - v2);
        SCL_EXPECT_TRUE(&tr, diff < 0.01);
    }

    scl_test_group("fbm");
    {
        scl_rand_noise_t noise;
        (void)scl_rand_noise_init(&noise, 777);
        double v = scl_rand_noise_fbm(&noise, 1.0, 2.0, 6, 2.0, 0.5);
        SCL_EXPECT_TRUE(&tr, v >= -1.0 && v <= 1.0);
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
