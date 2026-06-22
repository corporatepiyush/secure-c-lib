#include "scl_ringbuf.h"
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

static void test_no_overwrite(scl_test_runner_t *tr)
{
    scl_test_group("push and pop (no overwrite)");
    scl_allocator_t *a = scl_allocator_default();
    scl_ringbuf_t rb;
    scl_ringbuf_init(a, &rb, sizeof(int), 3, false);
    int v = 1; scl_ringbuf_push(&rb, &v);
    v = 2; scl_ringbuf_push(&rb, &v);
    v = 3; scl_ringbuf_push(&rb, &v);
    SCL_EXPECT_TRUE(tr, scl_ringbuf_full(&rb));
    SCL_EXPECT_ERROR(tr, scl_ringbuf_push(&rb, &v), SCL_ERR_FULL);
    scl_ringbuf_pop(&rb, &v); SCL_EXPECT_EQ_I(tr, v, 1);
    SCL_EXPECT_FALSE(tr, scl_ringbuf_full(&rb));
    scl_ringbuf_destroy(a, &rb);
}

static void test_overwrite(scl_test_runner_t *tr)
{
    scl_test_group("overwrite mode");
    scl_allocator_t *a = scl_allocator_default();
    scl_ringbuf_t rb;
    scl_ringbuf_init(a, &rb, sizeof(int), 2, true);
    int v1 = 1, v2 = 2, v3 = 3;
    scl_ringbuf_push(&rb, &v1);
    scl_ringbuf_push(&rb, &v2);
    scl_ringbuf_push(&rb, &v3);
    scl_ringbuf_pop(&rb, &v1); SCL_EXPECT_EQ_I(tr, v1, 2);
    scl_ringbuf_pop(&rb, &v2); SCL_EXPECT_EQ_I(tr, v2, 3);
    scl_ringbuf_destroy(a, &rb);
}

static void test_search(scl_test_runner_t *tr)
{
    scl_test_group("search");
    scl_allocator_t *a = scl_allocator_default();
    scl_ringbuf_t rb;
    scl_ringbuf_init(a, &rb, sizeof(int), 10, false);
    for (int i = 0; i < 10; i++) scl_ringbuf_push(&rb, &i);
    size_t idx;
    int key = 5;
    SCL_EXPECT_OK(tr, scl_ringbuf_search(&rb, &key, cmp_int, &idx));
    SCL_EXPECT_EQ_SZ(tr, idx, 5);
    key = 999;
    SCL_EXPECT_ERROR(tr, scl_ringbuf_search(&rb, &key, cmp_int, &idx), SCL_ERR_NOT_FOUND);
    scl_ringbuf_destroy(a, &rb);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("=== scl_ringbuf tests ===");
    test_no_overwrite(&tr);
    test_overwrite(&tr);
    test_search(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
