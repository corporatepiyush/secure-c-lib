/* Correctness tests for the TSV parser: tab-delimited fields, the field/row
 * iteration contract, backslash escape decoding (\t \n \r \\), and CRLF rows.
 * Fields alias the parser buffer and are not NUL-terminated. Run under ASan. */
#include "scl_test.h"
#include "scl_parse_tsv.h"
#include <string.h>

static int feq(const char *f, size_t n, const char *exp) {
    return n == strlen(exp) && memcmp(f, exp, n) == 0;
}

static void test_basic(scl_test_runner_t *tr) {
    scl_test_group("TSV: rows and fields, CRLF");
    scl_parse_tsv_t p;
    SCL_EXPECT_OK(tr, scl_parse_tsv_init(scl_allocator_default(), &p));
    SCL_EXPECT_OK(tr, scl_parse_tsv_feed(&p, "a\tb\tc\r\n1\t2\t3\n", 13));

    const char *f; size_t n;
    SCL_EXPECT_OK(tr, scl_parse_tsv_next_field(&p, &f, &n)); SCL_EXPECT_TRUE(tr, feq(f, n, "a"));
    SCL_EXPECT_OK(tr, scl_parse_tsv_next_field(&p, &f, &n)); SCL_EXPECT_TRUE(tr, feq(f, n, "b"));
    SCL_EXPECT_OK(tr, scl_parse_tsv_next_field(&p, &f, &n)); SCL_EXPECT_TRUE(tr, feq(f, n, "c"));
    SCL_EXPECT_TRUE(tr, scl_parse_tsv_next_field(&p, &f, &n) == SCL_ERR_EMPTY);
    SCL_EXPECT_OK(tr, scl_parse_tsv_next_row(&p));   /* consumes CRLF */
    SCL_EXPECT_OK(tr, scl_parse_tsv_next_field(&p, &f, &n)); SCL_EXPECT_TRUE(tr, feq(f, n, "1"));
    SCL_EXPECT_OK(tr, scl_parse_tsv_next_field(&p, &f, &n)); SCL_EXPECT_TRUE(tr, feq(f, n, "2"));
    SCL_EXPECT_OK(tr, scl_parse_tsv_next_field(&p, &f, &n)); SCL_EXPECT_TRUE(tr, feq(f, n, "3"));
    SCL_EXPECT_TRUE(tr, scl_parse_tsv_next_row(&p) == SCL_ERR_EMPTY);
    scl_parse_tsv_destroy(&p);
}

static void test_escapes(scl_test_runner_t *tr) {
    scl_test_group("TSV: backslash escape decoding");
    scl_parse_tsv_t p;
    SCL_EXPECT_OK(tr, scl_parse_tsv_init(scl_allocator_default(), &p));
    /* literal backslash-t, backslash-n, backslash-backslash, and an unknown
     * escape that must pass through untouched. */
    const char *in = "x\\ty\ta\\nb\tc\\\\d\te\\zf\n";
    SCL_EXPECT_OK(tr, scl_parse_tsv_feed(&p, in, strlen(in)));

    const char *f; size_t n;
    SCL_EXPECT_OK(tr, scl_parse_tsv_next_field(&p, &f, &n)); SCL_EXPECT_TRUE(tr, feq(f, n, "x\ty"));
    SCL_EXPECT_OK(tr, scl_parse_tsv_next_field(&p, &f, &n)); SCL_EXPECT_TRUE(tr, feq(f, n, "a\nb"));
    SCL_EXPECT_OK(tr, scl_parse_tsv_next_field(&p, &f, &n)); SCL_EXPECT_TRUE(tr, feq(f, n, "c\\d"));
    SCL_EXPECT_OK(tr, scl_parse_tsv_next_field(&p, &f, &n)); SCL_EXPECT_TRUE(tr, feq(f, n, "e\\zf"));
    SCL_EXPECT_TRUE(tr, scl_parse_tsv_next_row(&p) == SCL_ERR_EMPTY);
    scl_parse_tsv_destroy(&p);
}

static void test_empty_fields(scl_test_runner_t *tr) {
    scl_test_group("TSV: empty fields");
    scl_parse_tsv_t p;
    SCL_EXPECT_OK(tr, scl_parse_tsv_init(scl_allocator_default(), &p));
    SCL_EXPECT_OK(tr, scl_parse_tsv_feed(&p, "\tx\t\n", 4));

    const char *f; size_t n;
    SCL_EXPECT_OK(tr, scl_parse_tsv_next_field(&p, &f, &n)); SCL_EXPECT_TRUE(tr, feq(f, n, ""));
    SCL_EXPECT_OK(tr, scl_parse_tsv_next_field(&p, &f, &n)); SCL_EXPECT_TRUE(tr, feq(f, n, "x"));
    SCL_EXPECT_OK(tr, scl_parse_tsv_next_field(&p, &f, &n)); SCL_EXPECT_TRUE(tr, feq(f, n, ""));
    SCL_EXPECT_TRUE(tr, scl_parse_tsv_next_field(&p, &f, &n) == SCL_ERR_EMPTY);
    SCL_EXPECT_TRUE(tr, scl_parse_tsv_next_row(&p) == SCL_ERR_EMPTY);
    scl_parse_tsv_destroy(&p);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);
    test_basic(&tr);
    test_escapes(&tr);
    test_empty_fields(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
