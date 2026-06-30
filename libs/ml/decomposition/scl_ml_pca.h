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

#ifndef SCL_ML_PCA_H
#define SCL_ML_PCA_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_ml.h"

typedef struct {
    size_t        n_components;
    int           whiten;
    double        tol;
    size_t        max_sweeps;
    scl_allocator_t *alloc;
} scl_ml_pca_params_t;

#define SCL_ML_PCA_PARAMS_DEFAULT() \
    { .n_components = 0, .whiten = 0, .tol = 1e-8, .max_sweeps = 20, .alloc = NULL }

typedef struct scl_ml_pca scl_ml_pca_t;

SCL_WARN_UNUSED scl_error_t
scl_ml_pca_new(scl_ml_pca_t **model, scl_ml_pca_params_t params);

void
scl_ml_pca_free(scl_ml_pca_t *model);

SCL_WARN_UNUSED scl_error_t
scl_ml_pca_fit(scl_ml_pca_t *model, const scl_ml_dataset_t *ds);

SCL_WARN_UNUSED scl_error_t
scl_ml_pca_transform(scl_ml_pca_t *model, const scl_ml_dataset_t *ds,
                      SCL_ML_FLOAT *out);

SCL_WARN_UNUSED scl_error_t
scl_ml_pca_fit_transform(scl_ml_pca_t *model, const scl_ml_dataset_t *ds,
                          SCL_ML_FLOAT *out);

SCL_WARN_UNUSED scl_error_t
scl_ml_pca_inverse_transform(scl_ml_pca_t *model,
                              const SCL_ML_FLOAT *X_proj, size_t n_samples,
                              SCL_ML_FLOAT *out);

SCL_PURE size_t
scl_ml_pca_get_n_components(const scl_ml_pca_t *model);

SCL_WARN_UNUSED const SCL_ML_FLOAT *
scl_ml_pca_get_components(const scl_ml_pca_t *model);

SCL_WARN_UNUSED const SCL_ML_FLOAT *
scl_ml_pca_get_mean(const scl_ml_pca_t *model);

SCL_WARN_UNUSED const SCL_ML_FLOAT *
scl_ml_pca_get_explained_variance(const scl_ml_pca_t *model);

SCL_WARN_UNUSED const SCL_ML_FLOAT *
scl_ml_pca_get_explained_variance_ratio(const scl_ml_pca_t *model);

SCL_WARN_UNUSED scl_error_t
scl_ml_pca_save(const scl_ml_pca_t *model,
                 uint8_t **buf, size_t *len, scl_allocator_t *alloc);

SCL_WARN_UNUSED scl_error_t
scl_ml_pca_load(scl_ml_pca_t **model,
                 const uint8_t *buf, size_t len,
                 scl_ml_pca_params_t params);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif /* SCL_ML_PCA_H */
