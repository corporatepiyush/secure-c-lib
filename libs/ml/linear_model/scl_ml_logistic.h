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

/* Logistic Regression: binary classification via sigmoid + cross-entropy loss.
 * Solvers: SGD with mini-batch support, L1/L2/ElasticNet penalties. */

#ifndef SCL_ML_LOGISTIC_H
#define SCL_ML_LOGISTIC_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_ml.h"

typedef struct {
    scl_ml_solver_t   solver;           /* SCL_ML_SOLVER_SGD (default) */
    scl_ml_penalty_t   penalty;          /* SCL_ML_PENALTY_L2 (default) */
    double  alpha;                       /* Regularization strength (0 = none) */
    double  l1_ratio;                    /* ElasticNet mixing: 0 = pure L2, 1 = pure L1 */
    double  learning_rate;               /* SGD step size */
    double  tol;                         /* Convergence threshold on gradient norm */
    size_t  max_iter;                    /* Maximum number of epochs */
    size_t  batch_size;                  /* Mini-batch size (0 = full batch) */
    int     random_seed;                 /* -1 = use random device */
    int     verbose;                     /* >0 = print convergence info */
    int     n_threads;                   /* 0 = use hardware concurrency */
    scl_allocator_t *alloc;              /* NULL = use default allocator */
} scl_ml_logistic_params_t;

#define SCL_ML_LOGISTIC_PARAMS_DEFAULT() \
    { .solver = SCL_ML_SOLVER_SGD, .penalty = SCL_ML_PENALTY_L2, \
      .alpha = 0.0001, .l1_ratio = 0.5, .learning_rate = 0.01, \
      .tol = 1e-4, .max_iter = 1000, .batch_size = 32, \
      .random_seed = -1, .verbose = 0, .n_threads = 0, .alloc = NULL }

typedef struct scl_ml_logistic scl_ml_logistic_t;

SCL_WARN_UNUSED scl_error_t
scl_ml_logistic_new(scl_ml_logistic_t **model, scl_ml_logistic_params_t params);

void
scl_ml_logistic_free(scl_ml_logistic_t *model);

SCL_WARN_UNUSED scl_error_t
scl_ml_logistic_fit(scl_ml_logistic_t *model, const scl_ml_dataset_t *ds);

SCL_WARN_UNUSED scl_error_t
scl_ml_logistic_predict(scl_ml_logistic_t *model, const scl_ml_dataset_t *ds,
                         SCL_ML_FLOAT *y_out);

SCL_WARN_UNUSED scl_error_t
scl_ml_logistic_predict_proba(scl_ml_logistic_t *model, const scl_ml_dataset_t *ds,
                               SCL_ML_FLOAT *proba_out);

SCL_WARN_UNUSED const SCL_ML_FLOAT *
scl_ml_logistic_get_coef(const scl_ml_logistic_t *model);

SCL_WARN_UNUSED SCL_ML_FLOAT
scl_ml_logistic_get_intercept(const scl_ml_logistic_t *model);

SCL_PURE size_t
scl_ml_logistic_get_n_features(const scl_ml_logistic_t *model);

SCL_WARN_UNUSED scl_error_t
scl_ml_logistic_save(const scl_ml_logistic_t *model,
                      uint8_t **buf, size_t *len, scl_allocator_t *alloc);

SCL_WARN_UNUSED scl_error_t
scl_ml_logistic_load(scl_ml_logistic_t **model,
                      const uint8_t *buf, size_t len,
                      scl_ml_logistic_params_t params);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif /* SCL_ML_LOGISTIC_H */
