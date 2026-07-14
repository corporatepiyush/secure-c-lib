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

#ifndef SCL_ML_KMEANS_H
#define SCL_ML_KMEANS_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_ml.h"

typedef struct {
  size_t n_clusters;
  size_t max_iter;
  double tol;
  int n_init;
  int random_seed;
  int verbose;
  scl_allocator_t *alloc;
} scl_ml_kmeans_params_t;

#define SCL_ML_KMEANS_PARAMS_DEFAULT()                                         \
  {.n_clusters = 8,                                                            \
   .max_iter = 300,                                                            \
   .tol = 1e-4,                                                                \
   .n_init = 10,                                                               \
   .random_seed = -1,                                                          \
   .verbose = 0,                                                               \
   .alloc = NULL}

typedef struct scl_ml_kmeans scl_ml_kmeans_t;

SCL_WARN_UNUSED scl_error_t scl_ml_kmeans_new(scl_ml_kmeans_t **model,
                                              scl_ml_kmeans_params_t params);

void scl_ml_kmeans_free(scl_ml_kmeans_t *model);

SCL_WARN_UNUSED scl_error_t scl_ml_kmeans_fit(scl_ml_kmeans_t *model,
                                              const scl_ml_dataset_t *ds);

SCL_WARN_UNUSED scl_error_t scl_ml_kmeans_predict(scl_ml_kmeans_t *model,
                                                  const scl_ml_dataset_t *ds,
                                                  int *y_out);

SCL_PURE size_t scl_ml_kmeans_get_n_clusters(const scl_ml_kmeans_t *model);

SCL_PURE const int *scl_ml_kmeans_get_labels(const scl_ml_kmeans_t *model);

SCL_PURE SCL_ML_FLOAT scl_ml_kmeans_get_inertia(const scl_ml_kmeans_t *model);

SCL_WARN_UNUSED scl_error_t scl_ml_kmeans_save(const scl_ml_kmeans_t *model,
                                               uint8_t **buf, size_t *len,
                                               scl_allocator_t *alloc);

SCL_WARN_UNUSED scl_error_t scl_ml_kmeans_load(scl_ml_kmeans_t **model,
                                               const uint8_t *buf, size_t len,
                                               scl_ml_kmeans_params_t params);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif /* SCL_ML_KMEANS_H */
