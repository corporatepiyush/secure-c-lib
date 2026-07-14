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

#ifndef SCL_ML_RF_H
#define SCL_ML_RF_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_ml.h"

typedef struct {
  size_t n_estimators;
  size_t max_depth;
  size_t min_samples_split;
  size_t min_samples_leaf;
  size_t max_features;
  scl_ml_criterion_t criterion;
  int random_seed;
  int n_threads;
  int verbose;
  scl_allocator_t *alloc;
} scl_ml_rf_params_t;

#define SCL_ML_RF_PARAMS_DEFAULT()                                             \
  {.n_estimators = 100,                                                        \
   .max_depth = 0,                                                             \
   .min_samples_split = 2,                                                     \
   .min_samples_leaf = 1,                                                      \
   .max_features = 0,                                                          \
   .criterion = SCL_ML_CRITERION_GINI,                                         \
   .random_seed = -1,                                                          \
   .n_threads = 0,                                                             \
   .verbose = 0,                                                               \
   .alloc = NULL}

typedef struct scl_ml_rf scl_ml_rf_t;

SCL_WARN_UNUSED scl_error_t scl_ml_rf_new(scl_ml_rf_t **model,
                                          scl_ml_rf_params_t params);

void scl_ml_rf_free(scl_ml_rf_t *model);

SCL_WARN_UNUSED scl_error_t scl_ml_rf_fit(scl_ml_rf_t *model,
                                          const scl_ml_dataset_t *ds);

SCL_WARN_UNUSED scl_error_t scl_ml_rf_predict(scl_ml_rf_t *model,
                                              const scl_ml_dataset_t *ds,
                                              SCL_ML_FLOAT *y_out);

SCL_PURE size_t scl_ml_rf_get_n_features(const scl_ml_rf_t *model);

SCL_WARN_UNUSED scl_error_t scl_ml_rf_save(const scl_ml_rf_t *model,
                                           uint8_t **buf, size_t *len,
                                           scl_allocator_t *alloc);

SCL_WARN_UNUSED scl_error_t scl_ml_rf_load(scl_ml_rf_t **model,
                                           const uint8_t *buf, size_t len,
                                           scl_ml_rf_params_t params);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif /* SCL_ML_RF_H */
