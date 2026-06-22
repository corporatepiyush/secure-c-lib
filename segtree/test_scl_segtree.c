#include "scl_segtree.h"
#include "../../testlib/scl_test.h"

static void combine_int(void *out, const void *a, const void *b)
{
    *(int *)out = *(const int *)a + *(const int *)b;
}

static void test_basic(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    int data[] = {1, 2, 3, 4, 5};
    scl_segtree_t t;
    SCL_EXPECT_OK(tr, scl_segtree_init(alloc, &t, 5, sizeof(int), data, combine_int));

    int q;
    SCL_EXPECT_OK(tr, scl_segtree_query(&t, 0, 3, &q));
    SCL_EXPECT_EQ_I(tr, q, 1 + 2 + 3);

    SCL_EXPECT_OK(tr, scl_segtree_query(&t, 2, 5, &q));
    SCL_EXPECT_EQ_I(tr, q, 3 + 4 + 5);

    int v = 10;
    SCL_EXPECT_OK(tr, scl_segtree_update(&t, 2, &v));
    SCL_EXPECT_OK(tr, scl_segtree_query(&t, 0, 5, &q));
    SCL_EXPECT_EQ_I(tr, q, 1 + 2 + 10 + 4 + 5);

    scl_segtree_destroy(alloc, &t);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("scl_segtree tests");
    test_basic(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
