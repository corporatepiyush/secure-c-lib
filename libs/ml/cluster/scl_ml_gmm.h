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

#ifndef SCL_ML_GMM_H
#define SCL_ML_GMM_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_ml.h"

typedef struct {
    size_t        n_components;
    size_t        max_iter;
    double        tol;
    double        reg_covar;
    int           random_seed;
    scl_allocator_t *alloc;
} scl_ml_gmm_params_t;

#define SCL_ML_GMM_PARAMS_DEFAULT() \
    { .n_components = 3, .max_iter = 200, .tol = 1e-3, \
      .reg_covar = 1e-6, .random_seed = -1, .alloc = NULL }

typedef struct scl_ml_gmm scl_ml_gmm_t;

SCL_WARN_UNUSED scl_error_t
scl_ml_gmm_new(scl_ml_gmm_t **model, scl_ml_gmm_params_t params);

void
scl_ml_gmm_free(scl_ml_gmm_t *model);

SCL_WARN_UNUSED scl_error_t
scl_ml_gmm_fit(scl_ml_gmm_t *model, const scl_ml_dataset_t *ds);

SCL_WARN_UNUSED scl_error_t
scl_ml_gmm_predict(scl_ml_gmm_t *model, const scl_ml_dataset_t *ds,
                    size_t *labels_out);

SCL_WARN_UNUSED scl_error_t
scl_ml_gmm_predict_proba(scl_ml_gmm_t *model, const scl_ml_dataset_t *ds,
                          SCL_ML_FLOAT *proba_out);

SCL_WARN_UNUSED scl_error_t
scl_ml_gmm_save(const scl_ml_gmm_t *model,
                 uint8_t **buf, size_t *len, scl_allocator_t *alloc);

SCL_WARN_UNUSED scl_error_t
scl_ml_gmm_load(scl_ml_gmm_t **model,
                 const uint8_t *buf, size_t len,
                 scl_ml_gmm_params_t params);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif /* SCL_ML_GMM_H */
