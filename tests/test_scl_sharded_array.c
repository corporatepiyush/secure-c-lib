/* Tests for scl_sharded_array: dense stable indices, linear (one-shard-at-a-
 * time) growth, O(1) get, and correct teardown. Run under ASan. */
#include "scl_test.h"
#include "scl_sharded_array.h"
#include <string.h>

static void test_append_get(scl_test_runner_t *tr) {
    scl_test_group("sharded array: append/get with stable indices");
    scl_allocator_t *a = scl_allocator_default();
    scl_sharded_array_t sa;
    SCL_EXPECT_OK(tr, scl_sharded_array_init(a, &sa, sizeof(int), 4));
    SCL_EXPECT_EQ_SZ(tr, scl_sharded_array_count(&sa), 0);
    SCL_EXPECT_NULL(tr, scl_sharded_array_get(&sa, 0));   /* empty */

    int *firstptr = NULL;
    for (int i = 0; i < 10; i++) {
        size_t idx = 999;
        SCL_EXPECT_OK(tr, scl_sharded_array_append(&sa, &i, &idx));
        SCL_EXPECT_EQ_SZ(tr, idx, (size_t)i);            /* dense indices */
        if (i == 0) firstptr = (int *)scl_sharded_array_get(&sa, 0);
    }
    SCL_EXPECT_EQ_SZ(tr, scl_sharded_array_count(&sa), 10);

    /* Linear growth: 10 elements / 4 per shard => 3 shards. */
    SCL_EXPECT_EQ_SZ(tr, sa.shard_count, 3);

    /* Every value is retrievable and correct. */
    for (int i = 0; i < 10; i++) {
        int *p = (int *)scl_sharded_array_get(&sa, (size_t)i);
        SCL_EXPECT_NOT_NULL(tr, p);
        if (p) SCL_EXPECT_EQ_I(tr, *p, i);
    }
    /* Index 0's element never moved despite later appends (stable address). */
    SCL_EXPECT_EQ_PTR(tr, scl_sharded_array_get(&sa, 0), firstptr);
    SCL_EXPECT_NULL(tr, scl_sharded_array_get(&sa, 10));  /* one past end */

    scl_sharded_array_destroy(&sa);
    SCL_EXPECT_EQ_SZ(tr, scl_sharded_array_count(&sa), 0);
}

typedef struct { long a; long b; char tag; } rec_t;

static void test_struct_elems(scl_test_runner_t *tr) {
    scl_test_group("sharded array: struct elements + default shard length");
    scl_allocator_t *a = scl_allocator_default();
    scl_sharded_array_t sa;
    SCL_EXPECT_OK(tr, scl_sharded_array_init(a, &sa, sizeof(rec_t), 0));  /* default shard */
    SCL_EXPECT_EQ_SZ(tr, sa.shard_len, SCL_SHARDED_ARRAY_DEFAULT_SHARD);

    for (long i = 0; i < 1000; i++) {
        rec_t r = { i, i * 2, (char)(i & 0x7f) };
        SCL_EXPECT_OK(tr, scl_sharded_array_append(&sa, &r, NULL));
    }
    SCL_EXPECT_EQ_SZ(tr, scl_sharded_array_count(&sa), 1000);
    rec_t *r = (rec_t *)scl_sharded_array_get(&sa, 777);
    SCL_EXPECT_NOT_NULL(tr, r);
    if (r) { SCL_EXPECT_EQ_I(tr, r->a, 777); SCL_EXPECT_EQ_I(tr, r->b, 1554); }
    scl_sharded_array_destroy(&sa);
}

static void test_errors(scl_test_runner_t *tr) {
    scl_test_group("sharded array: argument validation");
    scl_allocator_t *a = scl_allocator_default();
    scl_sharded_array_t sa;
    SCL_EXPECT_TRUE(tr, scl_sharded_array_init(a, &sa, 0, 4) == SCL_ERR_INVALID_ARG);
    SCL_EXPECT_TRUE(tr, scl_sharded_array_init(a, NULL, 4, 4) == SCL_ERR_NULL_PTR);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);
    test_append_get(&tr);
    test_struct_elems(&tr);
    test_errors(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
