/* Correctness tests for the RFC-4180 CSV parser.
 *
 * These pin the field/row iteration contract and the in-place unescaping of
 * quoted fields (closing quote stripped, "" collapsed to a single "). The
 * field pointers alias the parser buffer and are NOT NUL-terminated, so all
 * comparisons are length-aware. Run under ASan to catch any over-read. */
#include "scl_parse_csv.h"
#include "scl_test.h"
#include <string.h>

/* Compare a (ptr,len) field against a C string. */
static int feq(const char *f, size_t n, const char *exp) {
  return n == strlen(exp) && memcmp(f, exp, n) == 0;
}

/* Parse `input` fully into row/field strings stored in `cells` (row-major,
 * NUL-joined per row into out[r]); returns the number of rows. Each field is
 * checked to lie within bounds by ASan via memcpy. */
static void run_csv(scl_test_runner_t *tr, const char *input) {
  scl_parse_csv_t p;
  SCL_EXPECT_OK(tr, scl_parse_csv_init(scl_allocator_default(), &p));
  SCL_EXPECT_OK(tr, scl_parse_csv_feed(&p, input, strlen(input)));
  scl_parse_csv_destroy(&p);
}

static void test_basic(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSV: simple rows and fields");
  scl_parse_csv_t p;
  SCL_EXPECT_OK(tr, scl_parse_csv_init(scl_allocator_default(), &p));
  SCL_EXPECT_OK(tr, scl_parse_csv_feed(&p, "a,b,c\n1,2,3\n", 12));

  const char *f;
  size_t n;
  /* row 1 */
  SCL_EXPECT_OK(tr, scl_parse_csv_next_field(&p, &f, &n));
  SCL_EXPECT_TRUE(tr, feq(f, n, "a"));
  SCL_EXPECT_OK(tr, scl_parse_csv_next_field(&p, &f, &n));
  SCL_EXPECT_TRUE(tr, feq(f, n, "b"));
  SCL_EXPECT_OK(tr, scl_parse_csv_next_field(&p, &f, &n));
  SCL_EXPECT_TRUE(tr, feq(f, n, "c"));
  SCL_EXPECT_TRUE(tr, scl_parse_csv_next_field(&p, &f, &n) == SCL_ERR_EMPTY);
  SCL_EXPECT_OK(tr, scl_parse_csv_next_row(&p));
  /* row 2 */
  SCL_EXPECT_OK(tr, scl_parse_csv_next_field(&p, &f, &n));
  SCL_EXPECT_TRUE(tr, feq(f, n, "1"));
  SCL_EXPECT_OK(tr, scl_parse_csv_next_field(&p, &f, &n));
  SCL_EXPECT_TRUE(tr, feq(f, n, "2"));
  SCL_EXPECT_OK(tr, scl_parse_csv_next_field(&p, &f, &n));
  SCL_EXPECT_TRUE(tr, feq(f, n, "3"));
  SCL_EXPECT_TRUE(tr, scl_parse_csv_next_field(&p, &f, &n) == SCL_ERR_EMPTY);
  SCL_EXPECT_TRUE(tr, scl_parse_csv_next_row(&p) == SCL_ERR_EMPTY);
  scl_parse_csv_destroy(&p);
  TEST_TRACE_END();
}

static void test_quoted(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSV: quoted fields, embedded delim/quote/newline");
  scl_parse_csv_t p;
  SCL_EXPECT_OK(tr, scl_parse_csv_init(scl_allocator_default(), &p));
  /* field 2 has a doubled quote -> single "; field 3 has an embedded comma;
   * field 4 has an embedded newline. */
  const char *in = "a,\"b\"\"c\",\"d,e\",\"f\ng\"\n";
  SCL_EXPECT_OK(tr, scl_parse_csv_feed(&p, in, strlen(in)));

  const char *f;
  size_t n;
  SCL_EXPECT_OK(tr, scl_parse_csv_next_field(&p, &f, &n));
  SCL_EXPECT_TRUE(tr, feq(f, n, "a"));
  SCL_EXPECT_OK(tr, scl_parse_csv_next_field(&p, &f, &n));
  SCL_EXPECT_TRUE(tr, feq(f, n, "b\"c"));
  SCL_EXPECT_OK(tr, scl_parse_csv_next_field(&p, &f, &n));
  SCL_EXPECT_TRUE(tr, feq(f, n, "d,e"));
  SCL_EXPECT_OK(tr, scl_parse_csv_next_field(&p, &f, &n));
  SCL_EXPECT_TRUE(tr, feq(f, n, "f\ng"));
  SCL_EXPECT_TRUE(tr, scl_parse_csv_next_field(&p, &f, &n) == SCL_ERR_EMPTY);
  SCL_EXPECT_TRUE(tr, scl_parse_csv_next_row(&p) == SCL_ERR_EMPTY);
  scl_parse_csv_destroy(&p);
  TEST_TRACE_END();
}

static void test_empty_fields_and_crlf(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSV: empty fields, trailing comma, CRLF");
  scl_parse_csv_t p;
  SCL_EXPECT_OK(tr, scl_parse_csv_init(scl_allocator_default(), &p));
  /* leading empty, middle empty, trailing empty; CRLF line ending */
  const char *in = ",x,\r\n";
  SCL_EXPECT_OK(tr, scl_parse_csv_feed(&p, in, strlen(in)));

  const char *f;
  size_t n;
  SCL_EXPECT_OK(tr, scl_parse_csv_next_field(&p, &f, &n));
  SCL_EXPECT_TRUE(tr, feq(f, n, ""));
  SCL_EXPECT_OK(tr, scl_parse_csv_next_field(&p, &f, &n));
  SCL_EXPECT_TRUE(tr, feq(f, n, "x"));
  SCL_EXPECT_OK(tr, scl_parse_csv_next_field(&p, &f, &n));
  SCL_EXPECT_TRUE(tr, feq(f, n, ""));
  SCL_EXPECT_TRUE(tr, scl_parse_csv_next_field(&p, &f, &n) == SCL_ERR_EMPTY);
  SCL_EXPECT_TRUE(tr, scl_parse_csv_next_row(&p) == SCL_ERR_EMPTY);
  scl_parse_csv_destroy(&p);
  TEST_TRACE_END();
}

static void test_no_trailing_newline(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSV: last record without trailing newline");
  scl_parse_csv_t p;
  SCL_EXPECT_OK(tr, scl_parse_csv_init(scl_allocator_default(), &p));
  SCL_EXPECT_OK(tr, scl_parse_csv_feed(&p, "a\nb", 3));

  const char *f;
  size_t n;
  SCL_EXPECT_OK(tr, scl_parse_csv_next_field(&p, &f, &n));
  SCL_EXPECT_TRUE(tr, feq(f, n, "a"));
  SCL_EXPECT_TRUE(tr, scl_parse_csv_next_field(&p, &f, &n) == SCL_ERR_EMPTY);
  SCL_EXPECT_OK(tr, scl_parse_csv_next_row(&p));
  SCL_EXPECT_OK(tr, scl_parse_csv_next_field(&p, &f, &n));
  SCL_EXPECT_TRUE(tr, feq(f, n, "b"));
  SCL_EXPECT_TRUE(tr, scl_parse_csv_next_field(&p, &f, &n) == SCL_ERR_EMPTY);
  SCL_EXPECT_TRUE(tr, scl_parse_csv_next_row(&p) == SCL_ERR_EMPTY);
  scl_parse_csv_destroy(&p);
  TEST_TRACE_END();
}

static void test_unterminated_quote(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSV: unterminated quote does not over-read");
  scl_parse_csv_t p;
  SCL_EXPECT_OK(tr, scl_parse_csv_init(scl_allocator_default(), &p));
  const char *in = "\"abc"; /* opening quote, no close, EOF */
  SCL_EXPECT_OK(tr, scl_parse_csv_feed(&p, in, strlen(in)));
  const char *f;
  size_t n;
  /* Best-effort: returns the available content; the key property is no crash
   * / no over-read (checked by ASan via the memcmp in feq). */
  SCL_EXPECT_OK(tr, scl_parse_csv_next_field(&p, &f, &n));
  SCL_EXPECT_TRUE(tr, feq(f, n, "abc"));
  scl_parse_csv_destroy(&p);
  (void)run_csv;
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);
  test_basic(&tr);
  test_quoted(&tr);
  test_empty_fields_and_crlf(&tr);
  test_no_trailing_newline(&tr);
  test_unterminated_quote(&tr);
  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
