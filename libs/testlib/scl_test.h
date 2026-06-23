#ifndef SCL_TEST_H
#define SCL_TEST_H

#include "scl_common.h"
#include <string.h>
#include <stdio.h>

/* ── Test state ─────────────────────────────────────────────── */
typedef struct {
    int passed;
    int failed;
    int asserts;
} scl_test_runner_t;

void scl_test_init(scl_test_runner_t *tr);

/* ── Test naming ────────────────────────────────────────────── */
void scl_test_group(const char *name);  /* prints group header */

/* ── Assertions (return true on success, false on failure) ─── */
bool scl_expect_true(scl_test_runner_t *tr, bool cond, const char *file, int line, const char *expr);
bool scl_expect_false(scl_test_runner_t *tr, bool cond, const char *file, int line, const char *expr);
bool scl_expect_eq_i(scl_test_runner_t *tr, int64_t a, int64_t b, const char *file, int line);
bool scl_expect_eq_u(scl_test_runner_t *tr, uint64_t a, uint64_t b, const char *file, int line);
bool scl_expect_eq_sz(scl_test_runner_t *tr, size_t a, size_t b, const char *file, int line);
bool scl_expect_eq_ptr(scl_test_runner_t *tr, const void *a, const void *b, const char *file, int line);
bool scl_expect_eq_str(scl_test_runner_t *tr, const char *a, const char *b, const char *file, int line);
bool scl_expect_eq_mem(scl_test_runner_t *tr, const void *a, const void *b, size_t n, const char *file, int line);
bool scl_expect_null(scl_test_runner_t *tr, const void *p, const char *file, int line);
bool scl_expect_not_null(scl_test_runner_t *tr, const void *p, const char *file, int line);
bool scl_expect_ok(scl_test_runner_t *tr, scl_error_t err, const char *file, int line);
bool scl_expect_error(scl_test_runner_t *tr, scl_error_t err, scl_error_t expected, const char *file, int line);

/* ── Summary ────────────────────────────────────────────────── */
void scl_test_summary(scl_test_runner_t *tr);

/* Convenience macros (file/line captured automatically) */
#define SCL_EXPECT_TRUE(tr, cond)       scl_expect_true(tr, !!(cond), __FILE__, __LINE__, #cond)
#define SCL_EXPECT_FALSE(tr, cond)      scl_expect_false(tr, !!(cond), __FILE__, __LINE__, #cond)
#define SCL_EXPECT_EQ_I(tr, a, b)       scl_expect_eq_i(tr, (int64_t)(a), (int64_t)(b), __FILE__, __LINE__)
#define SCL_EXPECT_EQ_U(tr, a, b)       scl_expect_eq_u(tr, (uint64_t)(a), (uint64_t)(b), __FILE__, __LINE__)
#define SCL_EXPECT_EQ_SZ(tr, a, b)      scl_expect_eq_sz(tr, (size_t)(a), (size_t)(b), __FILE__, __LINE__)
#define SCL_EXPECT_EQ_PTR(tr, a, b)     scl_expect_eq_ptr(tr, (const void *)(a), (const void *)(b), __FILE__, __LINE__)
#define SCL_EXPECT_EQ_STR(tr, a, b)     scl_expect_eq_str(tr, (a), (b), __FILE__, __LINE__)
#define SCL_EXPECT_NULL(tr, p)          scl_expect_null(tr, (p), __FILE__, __LINE__)
#define SCL_EXPECT_NOT_NULL(tr, p)      scl_expect_not_null(tr, (p), __FILE__, __LINE__)
#define SCL_EXPECT_OK(tr, err)          scl_expect_ok(tr, (err), __FILE__, __LINE__)
#define SCL_EXPECT_ERROR(tr, err, exp)  scl_expect_error(tr, (err), (exp), __FILE__, __LINE__)

#endif
