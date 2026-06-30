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

/* Distance metrics and kernel functions dispatched through SIMD. */

#ifndef SCL_ML_METRICS_H
#define SCL_ML_METRICS_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_ml.h"
#include "scl_ml_simd.h"

/* ── Distance computation ──────────────────────────────────────
 * All distances return computed value.
 * For squared L2, use scl_ml_simd.dist_l2_sq() directly. */

/* Batch distance matrix with user-selectable metric.
 * out[i * m + j] = distance between a[i*d..] and b[j*d..]. */
SCL_WARN_UNUSED scl_error_t
scl_ml_distance_matrix(SCL_ML_FLOAT *out,
                        const SCL_ML_FLOAT *a, const SCL_ML_FLOAT *b,
                        size_t n, size_t m, size_t d,
                        scl_ml_distance_t metric);

/* ── Kernel computation ──────────────────────────────────────── */
/* Compute K(x, y) for two vectors of length d */
SCL_PURE float
scl_ml_kernel(const SCL_ML_FLOAT *x, const SCL_ML_FLOAT *y,
              size_t d, scl_ml_kernel_t type,
              float gamma, float coef0, int degree);

/* Batch kernel matrix: K_out[i * m + j] = K(a[i], b[j]) */
SCL_WARN_UNUSED scl_error_t
scl_ml_kernel_matrix(SCL_ML_FLOAT *k_out,
                      const SCL_ML_FLOAT *a, const SCL_ML_FLOAT *b,
                      size_t n, size_t m, size_t d,
                      scl_ml_kernel_t type,
                      float gamma, float coef0, int degree);

/* ── Scoring / loss functions ────────────────────────────────── */
SCL_PURE float
scl_ml_mean_squared_error(const SCL_ML_FLOAT *y_true,
                           const SCL_ML_FLOAT *y_pred, size_t n);

SCL_PURE float
scl_ml_mean_absolute_error(const SCL_ML_FLOAT *y_true,
                            const SCL_ML_FLOAT *y_pred, size_t n);

SCL_PURE float
scl_ml_r2_score(const SCL_ML_FLOAT *y_true,
                 const SCL_ML_FLOAT *y_pred, size_t n);

SCL_PURE float
scl_ml_log_loss(const SCL_ML_FLOAT *y_true,
                 const SCL_ML_FLOAT *y_pred_proba, size_t n);

SCL_PURE float
scl_ml_accuracy(const float *y_true, const float *y_pred, size_t n);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif /* SCL_ML_METRICS_H */
