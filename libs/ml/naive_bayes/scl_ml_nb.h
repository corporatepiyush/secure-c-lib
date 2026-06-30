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

/* Gaussian Naive Bayes classifier.
 * Supports multi-class classification with per-class Gaussian density. */

#ifndef SCL_ML_NB_H
#define SCL_ML_NB_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_ml.h"

typedef struct {
    double  var_smoothing;               /* Added to variances for numerical stability (default 1e-9) */
    scl_allocator_t *alloc;              /* NULL = use default allocator */
} scl_ml_nb_params_t;

#define SCL_ML_NB_PARAMS_DEFAULT() \
    { .var_smoothing = 1e-9, .alloc = NULL }

typedef struct scl_ml_nb scl_ml_nb_t;

SCL_WARN_UNUSED scl_error_t
scl_ml_nb_new(scl_ml_nb_t **model, scl_ml_nb_params_t params);

void
scl_ml_nb_free(scl_ml_nb_t *model);

SCL_WARN_UNUSED scl_error_t
scl_ml_nb_fit(scl_ml_nb_t *model, const scl_ml_dataset_t *ds);

SCL_WARN_UNUSED scl_error_t
scl_ml_nb_predict(scl_ml_nb_t *model, const scl_ml_dataset_t *ds,
                   SCL_ML_FLOAT *y_out);

SCL_WARN_UNUSED scl_error_t
scl_ml_nb_predict_proba(scl_ml_nb_t *model, const scl_ml_dataset_t *ds,
                         SCL_ML_FLOAT *proba_out);

SCL_PURE size_t
scl_ml_nb_get_n_classes(const scl_ml_nb_t *model);

SCL_WARN_UNUSED scl_error_t
scl_ml_nb_save(const scl_ml_nb_t *model,
                uint8_t **buf, size_t *len, scl_allocator_t *alloc);

SCL_WARN_UNUSED scl_error_t
scl_ml_nb_load(scl_ml_nb_t **model,
                const uint8_t *buf, size_t len,
                scl_ml_nb_params_t params);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif /* SCL_ML_NB_H */
