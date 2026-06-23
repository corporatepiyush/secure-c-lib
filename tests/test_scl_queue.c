#include "scl_test.h"
#include "scl_queue.h"

static void test_queue_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("Queue: init and destroy");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_queue_t queue;

    scl_error_t err = scl_queue_init(alloc, &queue, sizeof(int), 10);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_queue_count(&queue), 0);
    SCL_EXPECT_TRUE(tr, scl_queue_empty(&queue));

    scl_queue_destroy(alloc, &queue);
}

static void test_queue_enqueue_dequeue(scl_test_runner_t *tr) {
    scl_test_group("Queue: enqueue and dequeue");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_queue_t queue;
    scl_queue_init(alloc, &queue, sizeof(int), 10);

    int values[] = {10, 20, 30};
    for (int i = 0; i < 3; i++) {
        scl_error_t err = scl_queue_enqueue(alloc, &queue, &values[i]);
        SCL_EXPECT_OK(tr, err);
    }
    SCL_EXPECT_EQ_SZ(tr, scl_queue_count(&queue), 3);

    int out;
    scl_error_t err = scl_queue_dequeue(&queue, &out);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_I(tr, out, 10);
    SCL_EXPECT_EQ_SZ(tr, scl_queue_count(&queue), 2);

    scl_queue_destroy(alloc, &queue);
}

static void test_queue_peek(scl_test_runner_t *tr) {
    scl_test_group("Queue: peek without dequeue");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_queue_t queue;
    scl_queue_init(alloc, &queue, sizeof(int), 10);

    int val = 42;
    scl_queue_enqueue(alloc, &queue, &val);

    int out;
    scl_error_t err = scl_queue_peek(&queue, &out);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_I(tr, out, 42);
    SCL_EXPECT_EQ_SZ(tr, scl_queue_count(&queue), 1);

    scl_queue_destroy(alloc, &queue);
}

static void test_queue_fifo_order(scl_test_runner_t *tr) {
    scl_test_group("Queue: FIFO ordering");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_queue_t queue;
    scl_queue_init(alloc, &queue, sizeof(int), 10);

    int values[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        scl_queue_enqueue(alloc, &queue, &values[i]);
    }

    for (int i = 0; i < 5; i++) {
        int out;
        scl_queue_dequeue(&queue, &out);
        SCL_EXPECT_EQ_I(tr, out, values[i]);
    }

    scl_queue_destroy(alloc, &queue);
}

static void test_queue_empty_checks(scl_test_runner_t *tr) {
    scl_test_group("Queue: empty checks");

    scl_allocator_t *alloc = scl_allocator_default();
    scl_queue_t queue;
    scl_queue_init(alloc, &queue, sizeof(int), 10);

    int out;
    scl_error_t err = scl_queue_dequeue(&queue, &out);
    SCL_EXPECT_TRUE(tr, err == SCL_ERR_EMPTY);

    scl_queue_destroy(alloc, &queue);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_queue_init_destroy(&tr);
    test_queue_enqueue_dequeue(&tr);
    test_queue_peek(&tr);
    test_queue_fifo_order(&tr);
    test_queue_empty_checks(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
