#include "scl_test.h"
#include "scl_ringbuf.h"

static void test_ringbuf_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("RingBuf: init and destroy");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_ringbuf_t rb;

    scl_error_t err = scl_ringbuf_init(alloc, &rb, sizeof(int), 10, false);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_ringbuf_count(&rb), 0);
    SCL_EXPECT_TRUE(tr, scl_ringbuf_empty(&rb));
    SCL_EXPECT_FALSE(tr, scl_ringbuf_full(&rb));

    scl_ringbuf_destroy(alloc, &rb);
}

static void test_ringbuf_push_pop(scl_test_runner_t *tr) {
    scl_test_group("RingBuf: push and pop");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_ringbuf_t rb;
    scl_ringbuf_init(alloc, &rb, sizeof(int), 10, false);

    int values[] = {10, 20, 30};
    for (int i = 0; i < 3; i++) {
        scl_error_t err = scl_ringbuf_push(&rb, &values[i]);
        SCL_EXPECT_OK(tr, err);
    }
    SCL_EXPECT_EQ_SZ(tr, scl_ringbuf_count(&rb), 3);

    int out;
    scl_error_t err = scl_ringbuf_pop(&rb, &out);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_I(tr, out, 10);
    SCL_EXPECT_EQ_SZ(tr, scl_ringbuf_count(&rb), 2);

    scl_ringbuf_destroy(alloc, &rb);
}

static void test_ringbuf_peek(scl_test_runner_t *tr) {
    scl_test_group("RingBuf: peek without pop");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_ringbuf_t rb;
    scl_ringbuf_init(alloc, &rb, sizeof(int), 10, false);

    int val = 42;
    scl_ringbuf_push(&rb, &val);

    int out;
    scl_error_t err = scl_ringbuf_peek(&rb, 0, &out);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_I(tr, out, 42);
    SCL_EXPECT_EQ_SZ(tr, scl_ringbuf_count(&rb), 1);

    scl_ringbuf_destroy(alloc, &rb);
}


int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_ringbuf_init_destroy(&tr);
    test_ringbuf_push_pop(&tr);
    test_ringbuf_peek(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
