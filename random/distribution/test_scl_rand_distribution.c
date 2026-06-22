#include "../../testlib/scl_test.h"
#include <math.h>
#include "scl_rand_distribution.h"

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    scl_test_group("uniform_range");
    {
        scl_rand_dist_t dist;
        (void)scl_rand_dist_init(&dist, 42);
        double min = 5.0, max = 10.0;
        int ok = 1;
        for (int i = 0; i < 1000; i++) {
            double v = scl_rand_dist_uniform(&dist, min, max);
            if (v < min || v >= max) { ok = 0; break; }
        }
        SCL_EXPECT_TRUE(&tr, ok);
    }

    scl_test_group("normal");
    {
        scl_rand_dist_t dist;
        (void)scl_rand_dist_init(&dist, 123);
        double sum = 0.0, sum_sq = 0.0;
        int n = 10000;
        for (int i = 0; i < n; i++) {
            double v = scl_rand_dist_normal(&dist, 0.0, 1.0);
            sum += v;
            sum_sq += v * v;
        }
        double avg = sum / n;
        double var = sum_sq / n - avg * avg;
        SCL_EXPECT_TRUE(&tr, fabs(avg) < 0.15);
        SCL_EXPECT_TRUE(&tr, fabs(var - 1.0) < 0.15);
    }

    scl_test_group("bernoulli");
    {
        scl_rand_dist_t dist;
        (void)scl_rand_dist_init(&dist, 77);
        double p = 0.3;
        int count = 0, trials = 10000;
        for (int i = 0; i < trials; i++)
            if (scl_rand_dist_bernoulli(&dist, p)) count++;
        double prop = (double)count / trials;
        SCL_EXPECT_TRUE(&tr, fabs(prop - p) < 0.04);
    }

    scl_test_group("exponential");
    {
        scl_rand_dist_t dist;
        (void)scl_rand_dist_init(&dist, 55);
        double lambda = 2.0;
        double sum = 0.0;
        int n = 5000;
        int ok = 1;
        for (int i = 0; i < n; i++) {
            double v = scl_rand_dist_exponential(&dist, lambda);
            if (v < 0.0) { ok = 0; break; }
            sum += v;
        }
        double avg = sum / n;
        SCL_EXPECT_TRUE(&tr, ok);
        SCL_EXPECT_TRUE(&tr, fabs(avg - 1.0 / lambda) < 0.1);
    }

    scl_test_group("shuffle");
    {
        scl_rand_dist_t dist;
        (void)scl_rand_dist_init(&dist, 222);
        int arr[100];
        for (int i = 0; i < 100; i++) arr[i] = i;
        SCL_EXPECT_OK(&tr, scl_rand_dist_shuffle(&dist, arr, 100, sizeof(int)));
        int sum = 0;
        for (int i = 0; i < 100; i++) sum += arr[i];
        SCL_EXPECT_EQ_I(&tr, sum, 99 * 100 / 2);
    }

    scl_test_group("sample");
    {
        scl_allocator_t *alloc = scl_allocator_default();
        scl_rand_dist_t dist;
        (void)scl_rand_dist_init(&dist, 333);
        size_t n = 50, k = 10;
        size_t *indices = (size_t *)scl_alloc(alloc, k * sizeof(size_t), _Alignof(max_align_t));
        SCL_EXPECT_NOT_NULL(&tr, indices);
        if (indices) {
            SCL_EXPECT_OK(&tr, scl_rand_dist_sample(alloc, &dist, n, k, indices));
            int ok = 1;
            for (size_t i = 0; i < k; i++) {
                if (indices[i] >= n) { ok = 0; break; }
                for (size_t j = i + 1; j < k; j++)
                    if (indices[i] == indices[j]) { ok = 0; break; }
            }
            SCL_EXPECT_TRUE(&tr, ok);
            scl_free(alloc, indices);
        }
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
