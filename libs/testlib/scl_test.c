/*
 * Copyright 2026 Piyush Katariya
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Test assertion macros: expect_true/false/eq_i/eq_u/eq_str/eq_mem/eq_ptr/null/not_null/lt/gt/le/ge. File+line reporting via __FILE__/__LINE__. */

#include "scl_test.h"
#include <inttypes.h>

void scl_test_init(scl_test_runner_t *tr) {
    tr->passed = 0;
    tr->failed = 0;
    tr->asserts = 0;
}

void scl_test_group(const char *name) {
    printf("\n─── %s ───\n", name);
}

static bool record(scl_test_runner_t *tr, bool ok, const char *file, int line,
                   const char *msg) {
    tr->asserts++;
    if (ok) {
        tr->passed++;
    } else {
        tr->failed++;
        fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, msg);
    }
    return ok;
}

bool scl_expect_true(scl_test_runner_t *tr, bool cond,
                     const char *file, int line, const char *expr) {
    if (cond) return record(tr, true, file, line, expr);
    char buf[256];
    snprintf(buf, sizeof(buf), "expected true: %s", expr);
    return record(tr, false, file, line, buf);
}

bool scl_expect_false(scl_test_runner_t *tr, bool cond,
                      const char *file, int line, const char *expr) {
    if (!cond) return record(tr, true, file, line, expr);
    char buf[256];
    snprintf(buf, sizeof(buf), "expected false: %s", expr);
    return record(tr, false, file, line, buf);
}

bool scl_expect_eq_i(scl_test_runner_t *tr, int64_t a, int64_t b,
                     const char *file, int line) {
    if (a == b) return record(tr, true, file, line, "");
    char buf[512];
    snprintf(buf, sizeof(buf), "expected %" PRId64 " == %" PRId64, a, b);
    return record(tr, false, file, line, buf);
}

bool scl_expect_eq_u(scl_test_runner_t *tr, uint64_t a, uint64_t b,
                     const char *file, int line) {
    if (a == b) return record(tr, true, file, line, "");
    char buf[512];
    snprintf(buf, sizeof(buf), "expected %" PRIu64 " == %" PRIu64, a, b);
    return record(tr, false, file, line, buf);
}

bool scl_expect_eq_sz(scl_test_runner_t *tr, size_t a, size_t b,
                      const char *file, int line) {
    if (a == b) return record(tr, true, file, line, "");
    char buf[512];
    snprintf(buf, sizeof(buf), "expected %zu == %zu", a, b);
    return record(tr, false, file, line, buf);
}

bool scl_expect_eq_ptr(scl_test_runner_t *tr, const void *a, const void *b,
                       const char *file, int line) {
    if (a == b) return record(tr, true, file, line, "");
    char buf[512];
    snprintf(buf, sizeof(buf), "expected ptr %p == %p", (const void *)a, (const void *)b);
    return record(tr, false, file, line, buf);
}

bool scl_expect_eq_str(scl_test_runner_t *tr, const char *a, const char *b,
                       const char *file, int line) {
    if ((a == NULL && b == NULL) || (a && b && strcmp(a, b) == 0))
        return record(tr, true, file, line, "");
    char buf[1024];
    snprintf(buf, sizeof(buf), "expected \"%s\" == \"%s\"",
             a ? a : "NULL", b ? b : "NULL");
    return record(tr, false, file, line, buf);
}

bool scl_expect_eq_mem(scl_test_runner_t *tr, const void *a, const void *b,
                       size_t n, const char *file, int line) {
    if ((a == NULL && b == NULL) || (a && b && memcmp(a, b, n) == 0))
        return record(tr, true, file, line, "");
    return record(tr, false, file, line, "memory mismatch");
}

bool scl_expect_null(scl_test_runner_t *tr, const void *p,
                     const char *file, int line) {
    if (p == NULL) return record(tr, true, file, line, "");
    return record(tr, false, file, line, "expected NULL");
}

bool scl_expect_not_null(scl_test_runner_t *tr, const void *p,
                         const char *file, int line) {
    if (p != NULL) return record(tr, true, file, line, "");
    return record(tr, false, file, line, "expected non-NULL");
}

bool scl_expect_ok(scl_test_runner_t *tr, scl_error_t err,
                   const char *file, int line) {
    if (err == SCL_OK) return record(tr, true, file, line, "");
    char buf[512];
    snprintf(buf, sizeof(buf), "expected OK, got %s", scl_error_string(err));
    return record(tr, false, file, line, buf);
}

bool scl_expect_error(scl_test_runner_t *tr, scl_error_t err,
                      scl_error_t expected, const char *file, int line) {
    if (err == expected) return record(tr, true, file, line, "");
    char buf[512];
    snprintf(buf, sizeof(buf), "expected %s, got %s",
             scl_error_string(expected), scl_error_string(err));
    return record(tr, false, file, line, buf);
}

void scl_test_summary(scl_test_runner_t *tr) {
    printf("\n=== %d passed, %d failed, %d total ===\n",
           tr->passed, tr->failed, tr->asserts);
}
