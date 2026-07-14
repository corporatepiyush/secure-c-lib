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

/* ML module master header: Dataset type, common enums, config parameter
 * structs, serialization helpers. */

#ifndef SCL_ML_H
#define SCL_ML_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"
#include "scl_math.h"
#include "scl_stdint.h"
#include <float.h>

/* ── Compile-time precision control ───────────────────────────
 * Define SCL_ML_USE_FLOAT64 before including this header to use
 * double precision throughout. Default is float (32-bit). */
#ifndef SCL_ML_FLOAT
#define SCL_ML_FLOAT float
#endif
#ifndef SCL_ML_FLOAT_PREFIX
#define SCL_ML_FLOAT_PREFIX(x) x##f
#endif
#ifndef SCL_ML_EPSILON
#define SCL_ML_EPSILON 1e-7f
#endif

/* ── SIMD alignment requirement ───────────────────────────────
 * All buffers fed to SIMD kernels must be 32-byte aligned (AVX2 requirement).
 * Use this for all model parameter, weight, and working buffer allocations. */
#define SCL_ML_ALIGNMENT 32

/* ── ML-specific error codes ──────────────────────────────────
 * Defined in scl_common.h alongside base error codes:
 *   SCL_ERR_ML_CONVERGENCE, SCL_ERR_ML_SINGULAR,
 *   SCL_ERR_ML_NO_SOLUTION, SCL_ERR_ML_EMPTY_CLUSTER,
 *   SCL_ERR_ML_MISSING_DATA, SCL_ERR_ML_OVERFLOW */

/* ── Common enums ────────────────────────────────────────────── */
typedef enum {
  SCL_ML_SOLVER_SGD,       /* Stochastic Gradient Descent */
  SCL_ML_SOLVER_CD,        /* Coordinate Descent */
  SCL_ML_SOLVER_SMO,       /* Sequential Minimal Optimization (SVM) */
  SCL_ML_SOLVER_EM,        /* Expectation-Maximization (GMM) */
  SCL_ML_SOLVER_NORMAL_EQ, /* Normal Equations (closed-form linear reg) */
  SCL_ML_SOLVER_AUTO       /* Auto-select best solver for data */
} scl_ml_solver_t;

typedef enum {
  SCL_ML_PENALTY_NONE,      /* No regularization */
  SCL_ML_PENALTY_L1,        /* Lasso / L1 regularization */
  SCL_ML_PENALTY_L2,        /* Ridge / L2 regularization */
  SCL_ML_PENALTY_ELASTICNET /* Combined L1 + L2 regularization */
} scl_ml_penalty_t;

typedef enum {
  SCL_ML_CRITERION_GINI,    /* Gini impurity (classification) */
  SCL_ML_CRITERION_ENTROPY, /* Shannon entropy (classification) */
  SCL_ML_CRITERION_MSE,     /* Mean squared error (regression) */
  SCL_ML_CRITERION_MAE      /* Mean absolute error (regression) */
} scl_ml_criterion_t;

typedef enum {
  SCL_ML_KERNEL_LINEAR,     /* Linear: K(x,y) = x · y */
  SCL_ML_KERNEL_RBF,        /* RBF:    K(x,y) = exp(-gamma * ||x-y||^2) */
  SCL_ML_KERNEL_POLY,       /* Poly:   K(x,y) = (gamma * x·y + coef0)^degree  */
  SCL_ML_KERNEL_SIGMOID,    /* Sigmoid: K(x,y) = tanh(gamma * x·y + coef0) */
  SCL_ML_KERNEL_PRECOMPUTED /* Precomputed kernel matrix */
} scl_ml_kernel_t;

typedef enum {
  SCL_ML_DISTANCE_L1,        /* Manhattan / cityblock */
  SCL_ML_DISTANCE_L2,        /* Euclidean (squared for efficiency) */
  SCL_ML_DISTANCE_COSINE,    /* 1 - cosine similarity */
  SCL_ML_DISTANCE_CHEBYSHEV, /* L-infinity / max norm */
  SCL_ML_DISTANCE_MINKOWSKI, /* Generalized L-p with parameter p */
  SCL_ML_DISTANCE_HAMMING    /* Hamming (proportion of differing bits) */
} scl_ml_distance_t;

typedef enum {
  SCL_ML_ALGO_LINEAR, /* SCERR_ML_ALGO_LINEAREGRESSION */
  SCL_ML_ALGO_LOGISTIC,
  SCL_ML_ALGO_TREE,
  SCL_ML_ALGO_RF,
  SCL_ML_ALGO_GBDT,
  SCL_ML_ALGO_SVM,
  SCL_ML_ALGO_KNN,
  SCL_ML_ALGO_NAIVE_BAYES,
  SCL_ML_ALGO_KMEANS,
  SCL_ML_ALGO_GMM,
  SCL_ML_ALGO_DBSCAN,
  SCL_ML_ALGO_PCA,
  SCL_ML_ALGO_COUNT
} scl_ml_algo_id_t;

/* ── Dataset ────────────────────────────────────────────────────
 * Holds training/inference data. Row-major layout by default.
 * Column-major view built lazily for efficient gradient computation. */
typedef struct {
  SCL_ML_FLOAT
  *data; /* Row-major: [n_rows * row_stride] floats. 32-byte aligned. */
  SCL_ML_FLOAT *targets; /* [n_rows] regression targets or class labels */
  size_t n_rows;         /* Number of samples */
  size_t n_cols;         /* Number of features */
  size_t row_stride;     /* Leading dimension of data (padded) */

  SCL_ML_FLOAT
  *data_col; /* Column-major view: [n_cols * col_stride], built lazily */
  size_t col_stride; /* Leading dimension of column-major view */

  uint8_t owns_data : 1;
  uint8_t owns_targets : 1;
  uint8_t owns_col : 1;
  uint8_t _pad : 5;
} scl_ml_dataset_t;

/* Initialize dataset with allocated empty buffer.
 * Caller must fill data/targets afterward. */
SCL_WARN_UNUSED scl_error_t scl_ml_dataset_init(scl_ml_dataset_t *ds,
                                                scl_allocator_t *alloc,
                                                size_t n_rows, size_t n_cols);

/* Wrap existing buffers (no copy). Dataset does NOT own the memory. */
SCL_WARN_UNUSED scl_error_t scl_ml_dataset_wrap(scl_ml_dataset_t *ds,
                                                SCL_ML_FLOAT *data,
                                                SCL_ML_FLOAT *targets,
                                                size_t n_rows, size_t n_cols);

/* Prepare internal structures: build column-major view if needed,
 * ensure alignment, validate data contains no NaN/Inf.
 * Must be called after populating data. */
SCL_WARN_UNUSED scl_error_t scl_ml_dataset_prepare(scl_ml_dataset_t *ds,
                                                   scl_allocator_t *alloc);

/* Free owned memory. Does NOT free wrapped (non-owned) buffers. */
void scl_ml_dataset_destroy(scl_ml_dataset_t *ds, scl_allocator_t *alloc);

/* Check if dataset contains any NaN or Inf values. */
SCL_WARN_UNUSED bool scl_ml_dataset_has_missing(const scl_ml_dataset_t *ds);

/* ── Serialization helpers ───────────────────────────────────── */
#define SCL_ML_MAGIC 0x53434C00u      /* "SCL\0" */
#define SCL_ML_FORMAT_VERSION 0x0100u /* major 1, minor 0 */

/* Serialization header — every saved model begins with this.
 * CRC32C covers the entire payload after the crc field. */
typedef struct {
  uint32_t magic;     /* SCL_ML_MAGIC */
  uint16_t version;   /* SCL_ML_FORMAT_VERSION */
  uint16_t algo_id;   /* scl_ml_algo_id_t */
  uint16_t header_sz; /* Total header bytes including this struct */
  uint16_t reserved;
  uint32_t crc32c; /* CRC32C of payload after this field */
} scl_ml_serial_header_t;

/* ── Serializer / Deserializer interface ───────────────────────
 * Each algorithm implements:
 *   scl_error_t scl_ml_<algo>_save(const model_t *, uint8_t **buf, size_t *len,
 * scl_allocator_t *); scl_error_t scl_ml_<algo>_load(model_t **, const uint8_t
 * *buf, size_t len, params_t);
 *
 * Buffer format:
 *   [serial_header][model_data][crc32c_payload]
 * All integers little-endian, pointers stored as relative offsets. */

/* ── Inline helpers ──────────────────────────────────────────── */
static inline int scl_ml_float_is_finite(SCL_ML_FLOAT x) { return isfinite(x); }

/* Compute CRC32C (Castagnoli polynomial 0x1EDC6F41) for data integrity */
uint32_t scl_ml_crc32c(const void *data, size_t len);

/* Clamp to avoid exp overflow/finite range in ML computations */
static inline SCL_ML_FLOAT scl_ml_clamp_logit(SCL_ML_FLOAT x) {
  if (scl_unlikely(x < -30.0f))
    return -30.0f;
  if (scl_unlikely(x > 30.0f))
    return 30.0f;
  return x;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif /* SCL_ML_H */
