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

#ifndef SCL_ML_SVM_H
#define SCL_ML_SVM_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_ml.h"

typedef struct {
    scl_ml_kernel_t kernel;
    int             degree;
    double          gamma;
    double          coef0;
    double          C;
    double          tol;
    int             max_iter;
    int             random_seed;
    int             verbose;
    scl_allocator_t *alloc;
} scl_ml_svm_params_t;

#define SCL_ML_SVM_PARAMS_DEFAULT() \
    { .kernel = SCL_ML_KERNEL_RBF, .degree = 3, .gamma = 0.0, \
      .coef0 = 0.0, .C = 1.0, .tol = 1e-3, .max_iter = -1, \
      .random_seed = -1, .verbose = 0, .alloc = NULL }

typedef struct scl_ml_svm scl_ml_svm_t;

SCL_WARN_UNUSED scl_error_t
scl_ml_svm_new(scl_ml_svm_t **model, scl_ml_svm_params_t params);

void
scl_ml_svm_free(scl_ml_svm_t *model);

SCL_WARN_UNUSED scl_error_t
scl_ml_svm_fit(scl_ml_svm_t *model, const scl_ml_dataset_t *ds);

SCL_WARN_UNUSED scl_error_t
scl_ml_svm_predict(scl_ml_svm_t *model, const scl_ml_dataset_t *ds,
                    SCL_ML_FLOAT *y_out);

SCL_PURE size_t
scl_ml_svm_get_n_features(const scl_ml_svm_t *model);

SCL_PURE size_t
scl_ml_svm_get_n_support(const scl_ml_svm_t *model);

SCL_WARN_UNUSED scl_error_t
scl_ml_svm_save(const scl_ml_svm_t *model,
                 uint8_t **buf, size_t *len, scl_allocator_t *alloc);

SCL_WARN_UNUSED scl_error_t
scl_ml_svm_load(scl_ml_svm_t **model,
                 const uint8_t *buf, size_t len,
                 scl_ml_svm_params_t params);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif /* SCL_ML_SVM_H */
