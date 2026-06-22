#include "scl_stack.h"
#include "../testlib/scl_test.h"
#include <string.h>

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static void test_basic(scl_test_runner_t *tr)
{
    scl_test_group("basic LIFO");
    scl_allocator_t *a = scl_allocator_default();
    scl_stack_t s;
    scl_stack_init(a, &s, sizeof(int), 0);
    SCL_EXPECT_TRUE(tr, scl_stack_empty(&s));
    for (int i = 0; i < 100; i++) SCL_EXPECT_OK(tr, scl_stack_push(a, &s, &i));
    SCL_EXPECT_EQ_SZ(tr, scl_stack_count(&s), 100);
    for (int i = 99; i >= 0; i--) {
        int v; scl_stack_pop(&s, &v); SCL_EXPECT_EQ_I(tr, v, i);
    }
    SCL_EXPECT_TRUE(tr, scl_stack_empty(&s));
    SCL_EXPECT_ERROR(tr, scl_stack_pop(&s, &(int){0}), SCL_ERR_EMPTY);
    scl_stack_destroy(a, &s);
}

static void test_search(scl_test_runner_t *tr)
{
    scl_test_group("search");
    scl_allocator_t *a = scl_allocator_default();
    scl_stack_t s;
    scl_stack_init(a, &s, sizeof(int), 0);
    for (int i = 0; i < 10; i++) scl_stack_push(a, &s, &i);
    size_t idx;
    int key = 5;
    SCL_EXPECT_OK(tr, scl_stack_search(&s, &key, cmp_int, &idx));
    SCL_EXPECT_EQ_SZ(tr, idx, 5);
    key = 999;
    SCL_EXPECT_ERROR(tr, scl_stack_search(&s, &key, cmp_int, &idx), SCL_ERR_NOT_FOUND);
    scl_stack_destroy(a, &s);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("=== scl_stack tests ===");
    test_basic(&tr);
    test_search(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
