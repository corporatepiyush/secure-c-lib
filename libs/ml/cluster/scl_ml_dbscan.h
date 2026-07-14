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

#ifndef SCL_ML_DBSCAN_H
#define SCL_ML_DBSCAN_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_ml.h"

typedef struct {
  double eps;
  size_t min_pts;
  int metric;
  scl_allocator_t *alloc;
} scl_ml_dbscan_params_t;

#define SCL_ML_DBSCAN_PARAMS_DEFAULT()                                         \
  {.eps = 0.5, .min_pts = 5, .metric = SCL_ML_DISTANCE_L2, .alloc = NULL}

typedef struct scl_ml_dbscan scl_ml_dbscan_t;

SCL_WARN_UNUSED scl_error_t scl_ml_dbscan_new(scl_ml_dbscan_t **model,
                                              scl_ml_dbscan_params_t params);

void scl_ml_dbscan_free(scl_ml_dbscan_t *model);

SCL_WARN_UNUSED scl_error_t scl_ml_dbscan_fit(scl_ml_dbscan_t *model,
                                              const scl_ml_dataset_t *ds);

SCL_WARN_UNUSED scl_error_t scl_ml_dbscan_predict(scl_ml_dbscan_t *model,
                                                  const scl_ml_dataset_t *ds,
                                                  int *y_out);

SCL_PURE size_t scl_ml_dbscan_get_n_clusters(const scl_ml_dbscan_t *model);

SCL_PURE const int *scl_ml_dbscan_get_labels(const scl_ml_dbscan_t *model);

SCL_WARN_UNUSED scl_error_t scl_ml_dbscan_save(const scl_ml_dbscan_t *model,
                                               uint8_t **buf, size_t *len,
                                               scl_allocator_t *alloc);

SCL_WARN_UNUSED scl_error_t scl_ml_dbscan_load(scl_ml_dbscan_t **model,
                                               const uint8_t *buf, size_t len,
                                               scl_ml_dbscan_params_t params);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif /* SCL_ML_DBSCAN_H */
