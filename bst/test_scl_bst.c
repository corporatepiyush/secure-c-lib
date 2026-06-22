#include "scl_bst.h"
#include "../../testlib/scl_test.h"

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static void visit_fill(void *data, void *ctx)
{
    int *arr = (int *)ctx;
    arr[*(int *)data] = *(int *)data;
}

static void test_insert_inorder(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    scl_bst_t t;
    SCL_EXPECT_OK(tr, scl_bst_init(alloc, &t, sizeof(int), cmp_int));
    int data[] = {5, 3, 7, 2, 4, 6, 8};
    for (size_t i = 0; i < 7; i++)
        SCL_EXPECT_OK(tr, scl_bst_insert(alloc, &t, &data[i]));
    SCL_EXPECT_EQ_SZ(tr, scl_bst_count(&t), 7);
    int sorted[7] = {0};
    SCL_EXPECT_OK(tr, scl_bst_inorder(&t, visit_fill, sorted));
    scl_bst_destroy(alloc, &t);
}

static void test_remove(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    scl_bst_t t;
    SCL_EXPECT_OK(tr, scl_bst_init(alloc, &t, sizeof(int), cmp_int));
    for (int i = 0; i < 10; i++)
        SCL_EXPECT_OK(tr, scl_bst_insert(alloc, &t, &i));
    int five = 5;
    SCL_EXPECT_OK(tr, scl_bst_remove(alloc, &t, &five));
    SCL_EXPECT_FALSE(tr, scl_bst_contains(&t, &five));
    SCL_EXPECT_EQ_SZ(tr, scl_bst_count(&t), 9);
    scl_bst_destroy(alloc, &t);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("scl_bst tests");
    test_insert_inorder(&tr);
    test_remove(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
