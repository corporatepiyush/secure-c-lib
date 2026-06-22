#include "scl_btree.h"
#include "../../testlib/scl_test.h"

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static void test_insert_get(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    scl_btree_t t;
    SCL_EXPECT_OK(tr, scl_btree_init(alloc, &t, sizeof(int), sizeof(int), 4, cmp_int));
    for (int i = 0; i < 50; i++)
        SCL_EXPECT_OK(tr, scl_btree_insert(alloc, &t, &i, &i));
    SCL_EXPECT_EQ_SZ(tr, scl_btree_count(&t), 50);
    for (int i = 0; i < 50; i++) {
        int v;
        SCL_EXPECT_OK(tr, scl_btree_get(&t, &i, &v));
        SCL_EXPECT_EQ_I(tr, v, i);
    }
    scl_btree_destroy(alloc, &t);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("scl_btree tests");
    test_insert_get(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
