#include "../../testlib/scl_test.h"
#include "scl_sort_counting_sort.h"

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_allocator_t *a = scl_allocator_default();
    scl_test_group("counting_sort");

    int32_t data[] = {5, 3, 8, 1, 9, 2, 7, 4, 6, 0};
    SCL_EXPECT_OK(&tr, scl_sort_counting_sort(a, data, 10));
    for (int32_t i = 0; i < 10; i++)
        SCL_EXPECT_EQ_I(&tr, i, data[i]);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
