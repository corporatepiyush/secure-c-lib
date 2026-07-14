/* Correctness tests for the JSON parser: typed values, nesting, object/array
 * access, \u UTF-8 decoding (incl. surrogate pairs), and that malformed input
 * and large trees free cleanly. Run under ASan+LSan to verify no leaks on the
 * error paths (the parser must free its partial tree on every failure). */
#include "scl_parse_json.h"
#include "scl_test.h"
#include <string.h>

static void test_scalars(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("JSON: scalar types");
  scl_allocator_t *a = scl_allocator_default();
  struct {
    const char *in;
    scl_parse_json_type_t ty;
  } cases[] = {
      {"true", SCL_JSON_BOOL},     {"false", SCL_JSON_BOOL},
      {"null", SCL_JSON_NULL},     {"42", SCL_JSON_INT64},
      {"-7", SCL_JSON_INT64},      {"3.5", SCL_JSON_DOUBLE},
      {"\"hi\"", SCL_JSON_STRING},
  };
  for (size_t i = 0; i < SCL_ARRAY_SIZE(cases); i++) {
    scl_parse_json_value_t *v = NULL;
    SCL_EXPECT_OK(tr, scl_parse_json_parse(a, cases[i].in, &v));
    SCL_EXPECT_NOT_NULL(tr, v);
    if (v)
      SCL_EXPECT_EQ_I(tr, scl_parse_json_get_type(v), cases[i].ty);
    scl_parse_json_free(a, v);
  }
  TEST_TRACE_END();
}

static void test_object_array(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("JSON: object/array access");
  scl_allocator_t *a = scl_allocator_default();
  scl_parse_json_value_t *root = NULL;
  const char *in =
      "{\"name\":\"x\",\"nums\":[10,20,30],\"ok\":true,\"f\":1.25}";
  SCL_EXPECT_OK(tr, scl_parse_json_parse(a, in, &root));
  SCL_EXPECT_NOT_NULL(tr, root);
  if (root) {
    SCL_EXPECT_EQ_I(tr, scl_parse_json_get_type(root), SCL_JSON_OBJECT);
    SCL_EXPECT_EQ_SZ(tr, scl_parse_json_object_len(root), 4);

    scl_parse_json_value_t *name = scl_parse_json_object_get(root, "name");
    SCL_EXPECT_NOT_NULL(tr, name);
    if (name)
      SCL_EXPECT_EQ_STR(tr, scl_parse_json_get_string(name), "x");

    scl_parse_json_value_t *nums = scl_parse_json_object_get(root, "nums");
    SCL_EXPECT_NOT_NULL(tr, nums);
    if (nums) {
      SCL_EXPECT_EQ_SZ(tr, scl_parse_json_array_len(nums), 3);
      SCL_EXPECT_EQ_I(
          tr, scl_parse_json_get_int(scl_parse_json_array_get(nums, 0)), 10);
      SCL_EXPECT_EQ_I(
          tr, scl_parse_json_get_int(scl_parse_json_array_get(nums, 2)), 30);
      SCL_EXPECT_NULL(tr, scl_parse_json_array_get(nums, 3)); /* OOB index */
    }
    SCL_EXPECT_EQ_I(
        tr, scl_parse_json_get_bool(scl_parse_json_object_get(root, "ok")), 1);
    SCL_EXPECT_TRUE(tr, scl_parse_json_get_double(
                            scl_parse_json_object_get(root, "f")) == 1.25);
    SCL_EXPECT_NULL(tr, scl_parse_json_object_get(root, "missing"));
  }
  scl_parse_json_free(a, root);
  TEST_TRACE_END();
}

static void test_unicode_escapes(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("JSON: \\u escapes decode to UTF-8");
  scl_allocator_t *a = scl_allocator_default();

  /* U+00E9 (é) -> 2-byte UTF-8 C3 A9; U+20AC (€) -> 3-byte E2 82 AC. */
  scl_parse_json_value_t *v = NULL;
  SCL_EXPECT_OK(tr, scl_parse_json_parse(a, "\"\\u00e9\\u20ac\"", &v));
  if (v)
    SCL_EXPECT_EQ_STR(tr, scl_parse_json_get_string(v), "\xc3\xa9\xe2\x82\xac");
  scl_parse_json_free(a, v);

  /* Surrogate pair for U+1F600 (😀) -> 4-byte F0 9F 98 80. */
  v = NULL;
  SCL_EXPECT_OK(tr, scl_parse_json_parse(a, "\"\\uD83D\\uDE00\"", &v));
  if (v)
    SCL_EXPECT_EQ_STR(tr, scl_parse_json_get_string(v), "\xf0\x9f\x98\x80");
  scl_parse_json_free(a, v);

  /* Lone high surrogate -> U+FFFD replacement (EF BF BD). */
  v = NULL;
  SCL_EXPECT_OK(tr, scl_parse_json_parse(a, "\"\\uD83D\"", &v));
  if (v)
    SCL_EXPECT_EQ_STR(tr, scl_parse_json_get_string(v), "\xef\xbf\xbd");
  scl_parse_json_free(a, v);
  TEST_TRACE_END();
}

static void test_malformed_no_leak(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("JSON: malformed input errors without leaking");
  scl_allocator_t *a = scl_allocator_default();
  /* Each of these allocates a partial tree before failing; the parser must
   * free it (LSan/ASan enforces this) and report an error. */
  const char *bad[] = {
      "{\"a\":1,\"b\":",        /* value missing */
      "{\"a\":1 \"b\":2}",      /* missing comma */
      "[1,2,",                  /* trailing comma + EOF */
      "{\"k\"}",                /* key with no value */
      "[[[[[[[[[[1",            /* deep unterminated nesting */
      "{\"a\":[{\"b\":\"x\"},", /* nested partial */
      "tru",                    /* truncated keyword */
      "[1 2]",                  /* missing comma between array elements */
      "{\"a\":1 \"b\":2}",      /* missing comma between object members */
  };
  for (size_t i = 0; i < SCL_ARRAY_SIZE(bad); i++) {
    scl_parse_json_value_t *v = NULL;
    scl_error_t e = scl_parse_json_parse(a, bad[i], &v);
    SCL_EXPECT_TRUE(tr, e != SCL_OK);
    scl_parse_json_free(a, v); /* v is NULL on error; safe */
  }
}

static void test_wide_tree_frees(scl_test_runner_t *tr) {
  scl_test_group("JSON: wide array frees fully (>4096 elements)");
  scl_allocator_t *a = scl_allocator_default();
  /* Build "[0,0,0,...]" with 5000 elements — exceeds the old 4096 free cap,
   * which used to leak the remainder. */
  size_t n = 5000;
  size_t cap = n * 2 + 4;
  char *buf = (char *)malloc(cap);
  SCL_EXPECT_NOT_NULL(tr, buf);
  if (!buf)
    return;
  size_t pos = 0;
  buf[pos++] = '[';
  for (size_t i = 0; i < n; i++) {
    buf[pos++] = '0';
    if (i + 1 < n)
      buf[pos++] = ',';
  }
  buf[pos++] = ']';
  buf[pos] = '\0';

  scl_parse_json_value_t *root = NULL;
  SCL_EXPECT_OK(tr, scl_parse_json_parse(a, buf, &root));
  if (root)
    SCL_EXPECT_EQ_SZ(tr, scl_parse_json_array_len(root), n);
  scl_parse_json_free(a, root);
  free(buf);
}

/* ── Counting allocator: portable leak check (macOS ASan has no LSan) ──── */
typedef struct {
  scl_allocator_t *base;
  long live;
} count_state_t;
static void *c_malloc(void *st, size_t sz, size_t al) {
  count_state_t *s = st;
  void *p = scl_alloc(s->base, sz, al);
  if (p)
    s->live++;
  return p;
}
static void *c_calloc(void *st, size_t n, size_t sz, size_t al) {
  count_state_t *s = st;
  void *p = scl_calloc(s->base, n, sz, al);
  if (p)
    s->live++;
  return p;
}
static void *c_realloc(void *st, void *ptr, size_t o, size_t n, size_t al) {
  count_state_t *s = st;
  void *p = scl_realloc(s->base, ptr, o, n, al);
  if (p && !ptr)
    s->live++; /* realloc(NULL) behaves like malloc */
  return p;
}
static void c_free(void *st, void *ptr) {
  count_state_t *s = st;
  scl_free(s->base, ptr);
  s->live--; /* only called for non-NULL */
}

static void test_no_leaks(scl_test_runner_t *tr) {
  scl_test_group("JSON: parse+free leaves zero outstanding allocations");
  count_state_t st = {scl_allocator_default(), 0};
  scl_allocator_t ca = {c_malloc, c_calloc, c_realloc, c_free, &st};

  const char *inputs[] = {
      /* valid */
      "{\"a\":1,\"b\":[2,3,{\"c\":\"\\u00e9\"}],\"d\":true}",
      "[1,2,3,4,5]",
      "\"plain\"",
      "{}",
      "[]",
      /* malformed (each builds a partial tree before failing) */
      "{\"a\":1,\"b\":",
      "[1,2,",
      "{\"k\"}",
      "[[[[[1",
      "{\"a\":[{\"b\":\"x\"},",
      "[1 2]",
      "{\"a\":1 \"b\":2}",
      "tru",
  };
  for (size_t i = 0; i < SCL_ARRAY_SIZE(inputs); i++) {
    scl_parse_json_value_t *v = NULL;
    scl_parse_json_parse(&ca, inputs[i], &v);
    scl_parse_json_free(&ca, v);
  }
  /* If any error path leaked its partial tree, live would be > 0. */
  SCL_EXPECT_EQ_I(tr, st.live, 0);
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);
  test_scalars(&tr);
  test_object_array(&tr);
  test_unicode_escapes(&tr);
  test_malformed_no_leak(&tr);
  test_wide_tree_frees(&tr);
  test_no_leaks(&tr);
  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
