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

/* Linear Regression: OLS, Ridge, Lasso, ElasticNet.
 * Solvers: SGD (mini-batch), Coordinate Descent (L1 path), Normal Equations. */

#ifndef SCL_ML_LINEAR_H
#define SCL_ML_LINEAR_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_ml.h"

/* ── Parameters ──────────────────────────────────────────────── */
typedef struct {
  scl_ml_solver_t solver; /* SCL_ML_SOLVER_SGD, _CD, _NORMAL_EQ, _AUTO */
  scl_ml_penalty_t
      penalty;            /* SCL_ML_PENALTY_NONE (OLS), _L1, _L2, _ELASTICNET */
  double alpha;           /* Regularization strength (0 = no regularization) */
  double l1_ratio;        /* ElasticNet mixing: 0 = pure L2, 1 = pure L1 */
  double learning_rate;   /* SGD step size (0 = auto: 1/sqrt(n_features)) */
  double tol;             /* Convergence: stop when gradient norm < tol */
  size_t max_iter;        /* Maximum number of passes over data */
  size_t batch_size;      /* Mini-batch size (0 = full batch) */
  int random_seed;        /* -1 = use random device */
  int verbose;            /* >0 = print convergence info */
  scl_allocator_t *alloc; /* NULL = use default allocator */
} scl_ml_linear_params_t;

#define SCL_ML_LINEAR_PARAMS_DEFAULT()                                         \
  {.solver = SCL_ML_SOLVER_AUTO,                                               \
   .penalty = SCL_ML_PENALTY_NONE,                                             \
   .alpha = 0.0,                                                               \
   .l1_ratio = 0.5,                                                            \
   .learning_rate = 0.0,                                                       \
   .tol = 1e-4,                                                                \
   .max_iter = 1000,                                                           \
   .batch_size = 32,                                                           \
   .random_seed = -1,                                                          \
   .verbose = 0,                                                               \
   .alloc = NULL}

/* Opaque handle */
typedef struct scl_ml_linear scl_ml_linear_t;

/* ── Lifecycle ───────────────────────────────────────────────── */
SCL_WARN_UNUSED scl_error_t scl_ml_linear_new(scl_ml_linear_t **model,
                                              scl_ml_linear_params_t params);

void scl_ml_linear_free(scl_ml_linear_t *model);

/* ── Training ───────────────────────────────────────────────── */
SCL_WARN_UNUSED scl_error_t scl_ml_linear_fit(scl_ml_linear_t *model,
                                              const scl_ml_dataset_t *ds);

/* ── Prediction ──────────────────────────────────────────────── */
/* y_out must be pre-allocated with n_samples entries */
SCL_WARN_UNUSED scl_error_t scl_ml_linear_predict(scl_ml_linear_t *model,
                                                  const scl_ml_dataset_t *ds,
                                                  SCL_ML_FLOAT *y_out);

/* ── Model inspection ───────────────────────────────────────── */
SCL_WARN_UNUSED SCL_ML_FLOAT
scl_ml_linear_get_intercept(const scl_ml_linear_t *model);

SCL_WARN_UNUSED const SCL_ML_FLOAT *
scl_ml_linear_get_weights(const scl_ml_linear_t *model);

SCL_PURE size_t scl_ml_linear_get_n_features(const scl_ml_linear_t *model);

/* ── Serialization ───────────────────────────────────────────── */
SCL_WARN_UNUSED scl_error_t scl_ml_linear_save(const scl_ml_linear_t *model,
                                               uint8_t **buf, size_t *len,
                                               scl_allocator_t *alloc);

SCL_WARN_UNUSED scl_error_t scl_ml_linear_load(scl_ml_linear_t **model,
                                               const uint8_t *buf, size_t len,
                                               scl_ml_linear_params_t params);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif /* SCL_ML_LINEAR_H */
