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

/* Test suite lifecycle macros (SCL_TEST_SUITE, SCL_TEST). Aggregate pass/fail
 * reporting across tests, non-zero exit code on failure. */

#ifndef SCL_TEST_RUNNER_H
#define SCL_TEST_RUNNER_H

#include "scl_stdbool.h"
#include "scl_stdint.h"
#include "scl_stdio.h"

/* Enhanced test framework with better reporting */

typedef struct {
  const char *name;
  int32_t passed;
  int32_t failed;
  int32_t skipped;
  int64_t start_time;
  int64_t end_time;
} scl_test_suite_t;

typedef struct {
  const char *suite_name;
  const char *test_name;
  const char *file;
  int32_t line;
  bool passed;
  const char *message;
} scl_test_result_t;

/* Test suite lifecycle */
scl_test_suite_t *scl_test_suite_create(const char *name);
void scl_test_suite_destroy(scl_test_suite_t *suite);

/* Test execution */
void scl_test_suite_begin_test(scl_test_suite_t *suite, const char *test_name);
void scl_test_suite_end_test(scl_test_suite_t *suite);

/* Assertion macros */
#define SCL_TEST_ASSERT(suite, expr, fmt, ...)                                 \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "  FAIL: " fmt "\n", __VA_ARGS__);                       \
      (suite)->failed++;                                                       \
    } else {                                                                   \
      (suite)->passed++;                                                       \
    }                                                                          \
  } while (0)

#define SCL_TEST_ASSERT_EQUAL(suite, expected, actual)                         \
  SCL_TEST_ASSERT(suite, (expected) == (actual), "Expected %ld, got %ld",      \
                  (long)(expected), (long)(actual))

#define SCL_TEST_ASSERT_NOT_NULL(suite, ptr)                                   \
  SCL_TEST_ASSERT(suite, (ptr) != NULL, "Expected non-NULL pointer")

#define SCL_TEST_ASSERT_NULL(suite, ptr)                                       \
  SCL_TEST_ASSERT(suite, (ptr) == NULL, "Expected NULL pointer")

#define SCL_TEST_ASSERT_OK(suite, err)                                         \
  SCL_TEST_ASSERT(suite, (err) == 0, "Expected SCL_OK, got %d", err)

#endif // SCL_TEST_RUNNER_H
