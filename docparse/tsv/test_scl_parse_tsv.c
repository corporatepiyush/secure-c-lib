#include "../../testlib/scl_test.h"
#include "scl_parse_tsv.h"

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    scl_allocator_t *alloc = scl_allocator_default();

    scl_test_group("init and destroy");
    {
        scl_parse_tsv_t tsv;
        SCL_EXPECT_OK(&tr, scl_parse_tsv_init(alloc, &tsv));
        scl_parse_tsv_destroy(&tsv);
    }

    scl_test_group("simple fields");
    {
        scl_parse_tsv_t tsv;
        scl_parse_tsv_init(alloc, &tsv);
        scl_parse_tsv_feed(&tsv, "a\tb\tc\n", 7);
        const char *f;
        size_t len;
        int ok = 1;
        ok = ok && (scl_parse_tsv_next_field(&tsv, &f, &len) == SCL_OK && len == 1 && f[0] == 'a');
        ok = ok && (scl_parse_tsv_next_field(&tsv, &f, &len) == SCL_OK && len == 1 && f[0] == 'b');
        ok = ok && (scl_parse_tsv_next_field(&tsv, &f, &len) == SCL_OK && len == 1 && f[0] == 'c');
        SCL_EXPECT_TRUE(&tr, ok);
        scl_parse_tsv_destroy(&tsv);
    }

    scl_test_group("multiple rows");
    {
        scl_parse_tsv_t tsv;
        scl_parse_tsv_init(alloc, &tsv);
        scl_parse_tsv_feed(&tsv, "a\tb\nc\td\n", 9);
        const char *f;
        size_t len;
        int ok = 1;
        ok = ok && (scl_parse_tsv_next_field(&tsv, &f, &len) == SCL_OK && len == 1 && f[0] == 'a');
        ok = ok && (scl_parse_tsv_next_field(&tsv, &f, &len) == SCL_OK && len == 1 && f[0] == 'b');
        ok = ok && (scl_parse_tsv_next_row(&tsv) == SCL_OK);
        ok = ok && (scl_parse_tsv_next_field(&tsv, &f, &len) == SCL_OK && len == 1 && f[0] == 'c');
        ok = ok && (scl_parse_tsv_next_field(&tsv, &f, &len) == SCL_OK && len == 1 && f[0] == 'd');
        SCL_EXPECT_TRUE(&tr, ok);
        scl_parse_tsv_destroy(&tsv);
    }

    scl_test_group("NULL checks");
    {
        SCL_EXPECT_TRUE(&tr, scl_parse_tsv_init(alloc, NULL) == SCL_ERR_NULL_PTR);
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
