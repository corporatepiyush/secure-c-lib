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

#include "scl_ml_scaler.h"
#include "scl_ml_simd.h"
#include <string.h>
#include <math.h>
#include <float.h>

/* ═══════════════════════════════════════════════════════════════
 * StandardScaler
 * ═══════════════════════════════════════════════════════════════ */

SCL_WARN_UNUSED scl_error_t
scl_ml_standard_scaler_new(scl_ml_standard_scaler_t **scaler) {
    if (scl_unlikely(!scaler)) return SCL_ERR_NULL_PTR;
    scl_allocator_t *alloc = scl_allocator_default();
    *scaler = (scl_ml_standard_scaler_t *)scl_calloc(
        alloc, 1, sizeof(scl_ml_standard_scaler_t), alignof(max_align_t));
    if (scl_unlikely(!*scaler)) return SCL_ERR_OUT_OF_MEMORY;
    (*scaler)->alloc = alloc;
    return SCL_OK;
}

void
scl_ml_standard_scaler_free(scl_ml_standard_scaler_t *scaler) {
    if (scl_unlikely(!scaler)) return;
    scl_allocator_t *a = scaler->alloc ? scaler->alloc : scl_allocator_default();
    scl_free(a, scaler->mean_);
    scl_free(a, scaler->std_);
    memset(scaler, 0, sizeof(*scaler));
    scl_free(a, scaler);
}

SCL_WARN_UNUSED scl_error_t
scl_ml_standard_scaler_fit(scl_ml_standard_scaler_t *scaler,
                            const scl_ml_dataset_t *ds) {
    if (scl_unlikely(!scaler || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!ds->data || ds->n_rows < 1)) return SCL_ERR_INVALID_ARG;
    scl_allocator_t *a = scaler->alloc ? scaler->alloc : scl_allocator_default();

    size_t nf = ds->n_cols;
    scaler->mean_ = (SCL_ML_FLOAT *)scl_calloc(a, nf, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    scaler->std_  = (SCL_ML_FLOAT *)scl_calloc(a, nf, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (scl_unlikely(!scaler->mean_ || !scaler->std_)) {
        scl_free(a, scaler->mean_); scl_free(a, scaler->std_);
        scaler->mean_ = scaler->std_ = NULL;
        return SCL_ERR_OUT_OF_MEMORY;
    }
    scaler->n_features = nf;

    /* Compute mean */
    for (size_t i = 0; i < ds->n_rows; i++)
        for (size_t j = 0; j < nf; j++)
            scaler->mean_[j] += ds->data[i * ds->row_stride + j];
    float inv_n = 1.0f / (float)ds->n_rows;
    for (size_t j = 0; j < nf; j++)
        scaler->mean_[j] *= inv_n;

    /* Compute std (population) */
    for (size_t i = 0; i < ds->n_rows; i++)
        for (size_t j = 0; j < nf; j++) {
            float diff = ds->data[i * ds->row_stride + j] - scaler->mean_[j];
            scaler->std_[j] += diff * diff;
        }
    float inv_n_std = 1.0f / (float)ds->n_rows;
    for (size_t j = 0; j < nf; j++) {
        scaler->std_[j] = sqrtf(scaler->std_[j] * inv_n_std);
        if (scaler->std_[j] < FLT_MIN)
            scaler->std_[j] = 1.0f; /* avoid div-by-zero */
    }

    scaler->fitted = 1;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_standard_scaler_transform(scl_ml_standard_scaler_t *scaler,
                                  scl_ml_dataset_t *ds) {
    if (scl_unlikely(!scaler || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!scaler->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(ds->n_cols != scaler->n_features)) return SCL_ERR_INVALID_ARG;

    for (size_t i = 0; i < ds->n_rows; i++)
        for (size_t j = 0; j < ds->n_cols; j++)
            ds->data[i * ds->row_stride + j] =
                (ds->data[i * ds->row_stride + j] - scaler->mean_[j]) / scaler->std_[j];

    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_standard_scaler_fit_transform(scl_ml_standard_scaler_t *scaler,
                                      scl_ml_dataset_t *ds) {
    scl_error_t err = scl_ml_standard_scaler_fit(scaler, ds);
    if (scl_unlikely(err != SCL_OK)) return err;
    return scl_ml_standard_scaler_transform(scaler, ds);
}

SCL_WARN_UNUSED scl_error_t
scl_ml_standard_scaler_inverse(scl_ml_standard_scaler_t *scaler,
                                scl_ml_dataset_t *ds) {
    if (scl_unlikely(!scaler || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!scaler->fitted)) return SCL_ERR_INVALID_STATE;

    for (size_t i = 0; i < ds->n_rows; i++)
        for (size_t j = 0; j < ds->n_cols; j++)
            ds->data[i * ds->row_stride + j] =
                ds->data[i * ds->row_stride + j] * scaler->std_[j] + scaler->mean_[j];

    return SCL_OK;
}

/* ═══════════════════════════════════════════════════════════════
 * MinMaxScaler
 * ═══════════════════════════════════════════════════════════════ */

SCL_WARN_UNUSED scl_error_t
scl_ml_minmax_scaler_new(scl_ml_minmax_scaler_t **scaler) {
    if (scl_unlikely(!scaler)) return SCL_ERR_NULL_PTR;
    scl_allocator_t *alloc = scl_allocator_default();
    *scaler = (scl_ml_minmax_scaler_t *)scl_calloc(
        alloc, 1, sizeof(scl_ml_minmax_scaler_t), alignof(max_align_t));
    if (scl_unlikely(!*scaler)) return SCL_ERR_OUT_OF_MEMORY;
    (*scaler)->alloc = alloc;
    return SCL_OK;
}

void
scl_ml_minmax_scaler_free(scl_ml_minmax_scaler_t *scaler) {
    if (scl_unlikely(!scaler)) return;
    scl_allocator_t *a = scaler->alloc ? scaler->alloc : scl_allocator_default();
    scl_free(a, scaler->min_);
    scl_free(a, scaler->scale_);
    memset(scaler, 0, sizeof(*scaler));
    scl_free(a, scaler);
}

SCL_WARN_UNUSED scl_error_t
scl_ml_minmax_scaler_fit(scl_ml_minmax_scaler_t *scaler,
                          const scl_ml_dataset_t *ds) {
    if (scl_unlikely(!scaler || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!ds->data || ds->n_rows < 1)) return SCL_ERR_INVALID_ARG;
    scl_allocator_t *a = scaler->alloc ? scaler->alloc : scl_allocator_default();

    size_t nf = ds->n_cols;
    scaler->min_   = (SCL_ML_FLOAT *)scl_calloc(a, nf, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    scaler->scale_ = (SCL_ML_FLOAT *)scl_calloc(a, nf, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (scl_unlikely(!scaler->min_ || !scaler->scale_)) {
        scl_free(a, scaler->min_); scl_free(a, scaler->scale_);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    scaler->n_features = nf;

    /* Initialize min/max with first row */
    for (size_t j = 0; j < nf; j++) {
        scaler->min_[j] = ds->data[j]; /* first row, column j */
    }
    SCL_ML_FLOAT *max = (SCL_ML_FLOAT *)scl_calloc(a, nf, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (scl_unlikely(!max)) { scl_free(a, scaler->min_); scl_free(a, scaler->scale_); return SCL_ERR_OUT_OF_MEMORY; }
    for (size_t j = 0; j < nf; j++)
        max[j] = ds->data[j];

    for (size_t i = 1; i < ds->n_rows; i++) {
        for (size_t j = 0; j < nf; j++) {
            SCL_ML_FLOAT v = ds->data[i * ds->row_stride + j];
            if (v < scaler->min_[j]) scaler->min_[j] = v;
            if (v > max[j]) max[j] = v;
        }
    }

    for (size_t j = 0; j < nf; j++) {
        SCL_ML_FLOAT range = max[j] - scaler->min_[j];
        scaler->scale_[j] = range > FLT_MIN ? 1.0f / range : 1.0f;
    }
    scl_free(a, max);

    scaler->fitted = 1;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_minmax_scaler_transform(scl_ml_minmax_scaler_t *scaler,
                                scl_ml_dataset_t *ds) {
    if (scl_unlikely(!scaler || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!scaler->fitted)) return SCL_ERR_INVALID_STATE;

    for (size_t i = 0; i < ds->n_rows; i++)
        for (size_t j = 0; j < ds->n_cols; j++)
            ds->data[i * ds->row_stride + j] =
                (ds->data[i * ds->row_stride + j] - scaler->min_[j]) * scaler->scale_[j];
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_minmax_scaler_fit_transform(scl_ml_minmax_scaler_t *scaler,
                                    scl_ml_dataset_t *ds) {
    scl_error_t err = scl_ml_minmax_scaler_fit(scaler, ds);
    if (scl_unlikely(err != SCL_OK)) return err;
    return scl_ml_minmax_scaler_transform(scaler, ds);
}

SCL_WARN_UNUSED scl_error_t
scl_ml_minmax_scaler_inverse(scl_ml_minmax_scaler_t *scaler,
                              scl_ml_dataset_t *ds) {
    if (scl_unlikely(!scaler || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!scaler->fitted)) return SCL_ERR_INVALID_STATE;

    for (size_t i = 0; i < ds->n_rows; i++)
        for (size_t j = 0; j < ds->n_cols; j++)
            ds->data[i * ds->row_stride + j] =
                ds->data[i * ds->row_stride + j] / scaler->scale_[j] + scaler->min_[j];
    return SCL_OK;
}
