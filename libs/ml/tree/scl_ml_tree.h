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

#ifndef SCL_ML_TREE_H
#define SCL_ML_TREE_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_ml.h"

typedef struct {
    size_t          max_depth;
    size_t          min_samples_split;
    size_t          min_samples_leaf;
    double          min_impurity_decrease;
    scl_ml_criterion_t criterion;
    size_t          max_features;
    int             random_seed;
    int             verbose;
    scl_allocator_t *alloc;
} scl_ml_tree_params_t;

#define SCL_ML_TREE_PARAMS_DEFAULT() \
    { .max_depth = 0, .min_samples_split = 2, .min_samples_leaf = 1, \
      .min_impurity_decrease = 0.0, .criterion = SCL_ML_CRITERION_GINI, \
      .max_features = 0, .random_seed = -1, .verbose = 0, .alloc = NULL }

typedef struct scl_ml_tree scl_ml_tree_t;

/* Node layout — exposed so ensembles (RF/GBDT) can lift a trained tree's
 * nodes with a single memcpy instead of a serialize/deserialize round-trip.
 * All scalar fields; no pointers → trivially relocatable. */
typedef struct {
    int           feature_idx;
    SCL_ML_FLOAT  threshold;
    int           left_child;
    int           right_child;
    SCL_ML_FLOAT  value;
    SCL_ML_FLOAT  impurity;
    size_t        n_samples;
} scl_ml_tree_node_t;

SCL_WARN_UNUSED scl_error_t
scl_ml_tree_new(scl_ml_tree_t **model, scl_ml_tree_params_t params);

void
scl_ml_tree_free(scl_ml_tree_t *model);

SCL_WARN_UNUSED scl_error_t
scl_ml_tree_fit(scl_ml_tree_t *model, const scl_ml_dataset_t *ds);

SCL_WARN_UNUSED scl_error_t
scl_ml_tree_predict(scl_ml_tree_t *model, const scl_ml_dataset_t *ds,
                     SCL_ML_FLOAT *y_out);

SCL_PURE size_t
scl_ml_tree_get_n_features(const scl_ml_tree_t *model);

SCL_PURE size_t
scl_ml_tree_get_n_nodes(const scl_ml_tree_t *model);

SCL_PURE size_t
scl_ml_tree_get_n_leaves(const scl_ml_tree_t *model);

/* Direct, zero-copy access to the node array. Valid until the tree is freed
 * or refit. Returns NULL if not fitted. */
SCL_PURE const scl_ml_tree_node_t *
scl_ml_tree_get_nodes(const scl_ml_tree_t *model);

SCL_WARN_UNUSED scl_error_t
scl_ml_tree_save(const scl_ml_tree_t *model,
                  uint8_t **buf, size_t *len, scl_allocator_t *alloc);

SCL_WARN_UNUSED scl_error_t
scl_ml_tree_load(scl_ml_tree_t **model,
                  const uint8_t *buf, size_t len,
                  scl_ml_tree_params_t params);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif /* SCL_ML_TREE_H */
