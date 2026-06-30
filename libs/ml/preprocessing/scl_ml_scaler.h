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

/* Feature scaling: StandardScaler (z-score), MinMaxScaler, RobustScaler. */

#ifndef SCL_ML_SCALER_H
#define SCL_ML_SCALER_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_ml.h"

/* ── StandardScaler (z-score normalization) ────────────────────
 * Standardize features by removing mean and scaling to unit variance:
 *   z = (x - mean) / std
 * std computed with ddof=0 (population std). */
typedef struct {
    SCL_ML_FLOAT *mean_;    /* [n_features] */
    SCL_ML_FLOAT *std_;     /* [n_features] */
    size_t        n_features;
    int           fitted;
} scl_ml_standard_scaler_t;

SCL_WARN_UNUSED scl_error_t
scl_ml_standard_scaler_new(scl_ml_standard_scaler_t **scaler);

void
scl_ml_standard_scaler_free(scl_ml_standard_scaler_t *scaler);

SCL_WARN_UNUSED scl_error_t
scl_ml_standard_scaler_fit(scl_ml_standard_scaler_t *scaler,
                            const scl_ml_dataset_t *ds);

/* Transform in-place: data[i] = (data[i] - mean[j]) / std[j] */
SCL_WARN_UNUSED scl_error_t
scl_ml_standard_scaler_transform(scl_ml_standard_scaler_t *scaler,
                                  scl_ml_dataset_t *ds);

/* Fit and transform in one call */
SCL_WARN_UNUSED scl_error_t
scl_ml_standard_scaler_fit_transform(scl_ml_standard_scaler_t *scaler,
                                      scl_ml_dataset_t *ds);

/* Inverse transform: restore original scale */
SCL_WARN_UNUSED scl_error_t
scl_ml_standard_scaler_inverse(scl_ml_standard_scaler_t *scaler,
                                scl_ml_dataset_t *ds);

/* ── MinMaxScaler (range normalization) ────────────────────────
 * Scale features to a given range [0, 1] by default:
 *   x_scaled = (x - min) / (max - min) */
typedef struct {
    SCL_ML_FLOAT *min_;     /* [n_features] */
    SCL_ML_FLOAT *scale_;   /* [n_features] = 1.0 / (max - min) */
    size_t        n_features;
    int           fitted;
} scl_ml_minmax_scaler_t;

SCL_WARN_UNUSED scl_error_t
scl_ml_minmax_scaler_new(scl_ml_minmax_scaler_t **scaler);

void
scl_ml_minmax_scaler_free(scl_ml_minmax_scaler_t *scaler);

SCL_WARN_UNUSED scl_error_t
scl_ml_minmax_scaler_fit(scl_ml_minmax_scaler_t *scaler,
                          const scl_ml_dataset_t *ds);

SCL_WARN_UNUSED scl_error_t
scl_ml_minmax_scaler_transform(scl_ml_minmax_scaler_t *scaler,
                                scl_ml_dataset_t *ds);

SCL_WARN_UNUSED scl_error_t
scl_ml_minmax_scaler_fit_transform(scl_ml_minmax_scaler_t *scaler,
                                    scl_ml_dataset_t *ds);

SCL_WARN_UNUSED scl_error_t
scl_ml_minmax_scaler_inverse(scl_ml_minmax_scaler_t *scaler,
                              scl_ml_dataset_t *ds);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif /* SCL_ML_SCALER_H */
