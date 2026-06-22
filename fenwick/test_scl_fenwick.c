#include "scl_fenwick.h"
#include "../../testlib/scl_test.h"

static void add_int(void *out, const void *a, const void *b)
{
    *(int *)out = *(const int *)a + *(const int *)b;
}

static void sub_int(void *out, const void *a, const void *b)
{
    *(int *)out = *(const int *)a - *(const int *)b;
}

static void test_basic(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    int data[] = {1, 2, 3, 4, 5};
    scl_fenwick_t fw;
    SCL_EXPECT_OK(tr, scl_fenwick_init(alloc, &fw, 5, sizeof(int), data, add_int, sub_int));

    int p;
    SCL_EXPECT_OK(tr, scl_fenwick_prefix(&fw, 2, &p));
    SCL_EXPECT_EQ_I(tr, p, 1 + 2 + 3);

    SCL_EXPECT_OK(tr, scl_fenwick_prefix(&fw, 4, &p));
    SCL_EXPECT_EQ_I(tr, p, 1 + 2 + 3 + 4 + 5);

    int q;
    SCL_EXPECT_OK(tr, scl_fenwick_range_query(&fw, 1, 3, &q));
    SCL_EXPECT_EQ_I(tr, q, 2 + 3 + 4);

    int d = 10;
    SCL_EXPECT_OK(tr, scl_fenwick_update(&fw, 2, &d));
    SCL_EXPECT_OK(tr, scl_fenwick_prefix(&fw, 4, &p));
    SCL_EXPECT_EQ_I(tr, p, 1 + 2 + 13 + 4 + 5);

    scl_fenwick_destroy(alloc, &fw);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("scl_fenwick tests");
    test_basic(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
