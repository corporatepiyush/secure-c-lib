#include "scl_sparse.h"
#include "../../testlib/scl_test.h"

static void combine_min(void *out, const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    *(int *)out = x < y ? x : y;
}

static void test_basic(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    int data[] = {3, 1, 4, 1, 5, 9, 2, 6};
    scl_sparse_t st;
    SCL_EXPECT_OK(tr, scl_sparse_init(alloc, &st, 8, sizeof(int), data, combine_min));

    int q;
    SCL_EXPECT_OK(tr, scl_sparse_query(&st, 0, 3, &q));
    SCL_EXPECT_EQ_I(tr, q, 1);

    SCL_EXPECT_OK(tr, scl_sparse_query(&st, 2, 5, &q));
    SCL_EXPECT_EQ_I(tr, q, 1);

    SCL_EXPECT_OK(tr, scl_sparse_query(&st, 4, 7, &q));
    SCL_EXPECT_EQ_I(tr, q, 2);

    SCL_EXPECT_OK(tr, scl_sparse_query(&st, 0, 0, &q));
    SCL_EXPECT_EQ_I(tr, q, 3);

    scl_sparse_destroy(alloc, &st);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("scl_sparse tests");
    test_basic(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
