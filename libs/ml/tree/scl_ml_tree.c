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

#include "scl_ml_tree.h"
#include "scl_ml_simd.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

typedef struct {
    int           feature_idx;
    SCL_ML_FLOAT  threshold;
    int           left_child;
    int           right_child;
    SCL_ML_FLOAT  value;
    SCL_ML_FLOAT  impurity;
    size_t        n_samples;
} scl_ml_tree_node_t;

typedef struct scl_ml_tree {
    scl_ml_tree_params_t params;
    scl_ml_tree_node_t  *nodes;
    size_t               n_nodes;
    size_t               n_capacity;
    size_t               n_leaves;
    size_t               n_features;
    size_t               n_classes;
    int                  fitted;
    int                  is_classifier;
} scl_ml_tree_t;

static const size_t SCL_TREE_INIT_CAPACITY = 64;

static int
scl_ml_tree_ensure_capacity(scl_ml_tree_t *tree, size_t needed) {
    if (needed <= tree->n_capacity) return 0;
    size_t new_cap = tree->n_capacity ? tree->n_capacity * 2 : SCL_TREE_INIT_CAPACITY;
    while (new_cap < needed) new_cap *= 2;
    scl_ml_tree_node_t *p = (scl_ml_tree_node_t *)realloc(tree->nodes,
        new_cap * sizeof(scl_ml_tree_node_t));
    if (!p) return -1;
    memset(p + tree->n_capacity, 0,
           (new_cap - tree->n_capacity) * sizeof(scl_ml_tree_node_t));
    tree->nodes = p;
    tree->n_capacity = new_cap;
    return 0;
}

static int
scl_ml_tree_add_node(scl_ml_tree_t *tree, int *node_idx) {
    if (scl_ml_tree_ensure_capacity(tree, tree->n_nodes + 1) != 0)
        return -1;
    *node_idx = (int)tree->n_nodes;
    tree->n_nodes++;
    return 0;
}

static SCL_ML_FLOAT
scl_ml_compute_gini(const SCL_ML_FLOAT *targets,
                     const size_t *indices, size_t n,
                     size_t n_classes) {
    if (n == 0) return 0.0f;
    size_t *counts = (size_t *)calloc(n_classes, sizeof(size_t));
    if (!counts) return 1.0f;
    for (size_t i = 0; i < n; i++) {
        int cls = (int)targets[indices[i]];
        if (cls >= 0 && (size_t)cls < n_classes)
            counts[cls]++;
    }
    double sum_sq = 0.0;
    for (size_t k = 0; k < n_classes; k++) {
        double p = (double)counts[k] / (double)n;
        sum_sq += p * p;
    }
    free(counts);
    return (SCL_ML_FLOAT)(1.0 - sum_sq);
}

static SCL_ML_FLOAT
scl_ml_compute_mse(const SCL_ML_FLOAT *targets,
                    const size_t *indices, size_t n) {
    if (n == 0) return 0.0f;
    double mean = 0.0;
    for (size_t i = 0; i < n; i++)
        mean += (double)targets[indices[i]];
    mean /= (double)n;
    double mse = 0.0;
    for (size_t i = 0; i < n; i++) {
        double d = (double)targets[indices[i]] - mean;
        mse += d * d;
    }
    return (SCL_ML_FLOAT)(mse / (double)n);
}

static SCL_ML_FLOAT
scl_ml_compute_impurity(const SCL_ML_FLOAT *targets,
                         const size_t *indices, size_t n,
                         int is_classifier, size_t n_classes) {
    if (is_classifier)
        return scl_ml_compute_gini(targets, indices, n, n_classes);
    return scl_ml_compute_mse(targets, indices, n);
}

static SCL_ML_FLOAT
scl_ml_compute_leaf_value(const SCL_ML_FLOAT *targets,
                           const size_t *indices, size_t n,
                           int is_classifier, size_t n_classes) {
    if (!is_classifier) {
        if (n == 0) return 0.0f;
        double sum = 0.0;
        for (size_t i = 0; i < n; i++)
            sum += (double)targets[indices[i]];
        return (SCL_ML_FLOAT)(sum / (double)n);
    }
    if (n == 0) return 0.0f;
    size_t *counts = (size_t *)calloc(n_classes, sizeof(size_t));
    if (!counts) return 0.0f;
    for (size_t i = 0; i < n; i++) {
        int cls = (int)targets[indices[i]];
        if (cls >= 0 && (size_t)cls < n_classes)
            counts[cls]++;
    }
    size_t best_idx = 0;
    size_t best_count = counts[0];
    for (size_t k = 1; k < n_classes; k++) {
        if (counts[k] > best_count) {
            best_count = counts[k];
            best_idx = k;
        }
    }
    free(counts);
    return (SCL_ML_FLOAT)best_idx;
}

typedef struct {
    SCL_ML_FLOAT value;
    SCL_ML_FLOAT target;
    size_t       orig_idx;
} scl_ml_sort_pair_t;

static int
scl_ml_sort_pair_cmp(const void *a, const void *b) {
    const scl_ml_sort_pair_t *pa = (const scl_ml_sort_pair_t *)a;
    const scl_ml_sort_pair_t *pb = (const scl_ml_sort_pair_t *)b;
    if (pa->value < pb->value) return -1;
    if (pa->value > pb->value) return 1;
    return 0;
}

static int
scl_ml_tree_find_best_split(
    const scl_ml_dataset_t *ds,
    const size_t *indices, size_t n,
    int is_classifier, size_t n_classes,
    const size_t *feature_subset, size_t n_subset,
    size_t min_samples_leaf, double min_impurity_decrease,
    SCL_ML_FLOAT current_impurity,
    int *best_feature, SCL_ML_FLOAT *best_threshold,
    SCL_ML_FLOAT *best_impurity_reduction) {

    *best_feature = -1;
    *best_threshold = 0.0f;
    *best_impurity_reduction = 0.0f;
    if (n <= 1) return 0;

    double best_reduction = -1.0;

    scl_ml_sort_pair_t *pairs =
        (scl_ml_sort_pair_t *)malloc(n * sizeof(scl_ml_sort_pair_t));
    if (!pairs) return -1;

    for (size_t f = 0; f < n_subset; f++) {
        size_t feat = feature_subset[f];

        for (size_t i = 0; i < n; i++) {
            size_t row = indices[i];
            pairs[i].value = ds->data[row * ds->row_stride + feat];
            pairs[i].target = ds->targets[row];
            pairs[i].orig_idx = indices[i];
        }

        qsort(pairs, n, sizeof(scl_ml_sort_pair_t), scl_ml_sort_pair_cmp);

        size_t *left_indices = (size_t *)malloc(n * sizeof(size_t));
        if (!left_indices) { free(pairs); return -1; }
        size_t *right_indices = (size_t *)malloc(n * sizeof(size_t));
        if (!right_indices) { free(pairs); free(left_indices); return -1; }

        for (size_t split_i = 0; split_i < n - 1; split_i++) {
            if (pairs[split_i].value == pairs[split_i + 1].value)
                continue;

            size_t n_left = split_i + 1;
            size_t n_right = n - n_left;

            if (n_left < min_samples_leaf || n_right < min_samples_leaf)
                continue;

            for (size_t j = 0; j <= split_i; j++)
                left_indices[j] = pairs[j].orig_idx;
            for (size_t j = split_i + 1; j < n; j++)
                right_indices[j - split_i - 1] = pairs[j].orig_idx;

            SCL_ML_FLOAT left_imp = scl_ml_compute_impurity(
                ds->targets, left_indices, n_left,
                is_classifier, n_classes);
            SCL_ML_FLOAT right_imp = scl_ml_compute_impurity(
                ds->targets, right_indices, n_right,
                is_classifier, n_classes);

            double reduction = (double)current_impurity -
                ((double)n_left / (double)n) * (double)left_imp -
                ((double)n_right / (double)n) * (double)right_imp;

            if (reduction > best_reduction) {
                best_reduction = reduction;
                *best_feature = (int)feat;
                *best_threshold = (pairs[split_i].value +
                                   pairs[split_i + 1].value) / 2.0f;
                *best_impurity_reduction = (SCL_ML_FLOAT)reduction;
            }
        }

        free(left_indices);
        free(right_indices);
    }

    free(pairs);

    if (*best_feature < 0) return 0;
    if ((double)*best_impurity_reduction < min_impurity_decrease) {
        *best_feature = -1;
        return 0;
    }
    return 0;
}

static int
scl_ml_tree_grow(
    scl_ml_tree_t *tree,
    const scl_ml_dataset_t *ds,
    size_t *indices, size_t n,
    size_t depth,
    const size_t *feature_subset, size_t n_subset) {

    int node_idx;
    if (scl_ml_tree_add_node(tree, &node_idx) != 0) return -1;

    scl_ml_tree_node_t *node = &tree->nodes[node_idx];
    node->feature_idx = -1;
    node->left_child = -1;
    node->right_child = -1;
    node->threshold = 0.0f;
    node->n_samples = n;

    node->impurity = scl_ml_compute_impurity(
        ds->targets, indices, n, tree->is_classifier, tree->n_classes);

    int is_leaf = 0;
    if (n < tree->params.min_samples_split ||
        n < 2 * tree->params.min_samples_leaf) {
        is_leaf = 1;
    }
    if (tree->params.max_depth > 0 && depth >= tree->params.max_depth)
        is_leaf = 1;
    if (node->impurity < SCL_ML_EPSILON)
        is_leaf = 1;

    if (is_leaf) {
        node->value = scl_ml_compute_leaf_value(
            ds->targets, indices, n, tree->is_classifier, tree->n_classes);
        tree->n_leaves++;
        return 0;
    }

    int best_feature;
    SCL_ML_FLOAT best_threshold;
    SCL_ML_FLOAT best_reduction;
    int ret = scl_ml_tree_find_best_split(
        ds, indices, n,
        tree->is_classifier, tree->n_classes,
        feature_subset, n_subset,
        tree->params.min_samples_leaf,
        tree->params.min_impurity_decrease,
        node->impurity,
        &best_feature, &best_threshold, &best_reduction);

    if (ret != 0) return -1;

    if (best_feature < 0) {
        node->value = scl_ml_compute_leaf_value(
            ds->targets, indices, n, tree->is_classifier, tree->n_classes);
        tree->n_leaves++;
        return 0;
    }

    size_t n_left = 0, n_right = 0;
    for (size_t i = 0; i < n; i++) {
        size_t row = indices[i];
        if (ds->data[row * ds->row_stride + best_feature] <= best_threshold)
            n_left++;
        else
            n_right++;
    }

    size_t *left_idx = (size_t *)malloc(n_left * sizeof(size_t));
    size_t *right_idx = (size_t *)malloc(n_right * sizeof(size_t));
    if (!left_idx || !right_idx) {
        free(left_idx); free(right_idx);
        return -1;
    }

    size_t li = 0, ri = 0;
    for (size_t i = 0; i < n; i++) {
        size_t row = indices[i];
        if (ds->data[row * ds->row_stride + best_feature] <= best_threshold)
            left_idx[li++] = row;
        else
            right_idx[ri++] = row;
    }

    node = &tree->nodes[node_idx];
    node->feature_idx = best_feature;
    node->threshold = best_threshold;

    int left_child = (int)tree->n_nodes;
    if (scl_ml_tree_grow(tree, ds, left_idx, n_left, depth + 1,
                          feature_subset, n_subset) != 0) {
        free(left_idx); free(right_idx);
        return -1;
    }
    node = &tree->nodes[node_idx];
    node->left_child = left_child;

    int right_child = (int)tree->n_nodes;
    if (scl_ml_tree_grow(tree, ds, right_idx, n_right, depth + 1,
                          feature_subset, n_subset) != 0) {
        free(left_idx); free(right_idx);
        return -1;
    }
    node = &tree->nodes[node_idx];
    node->right_child = right_child;

    free(left_idx);
    free(right_idx);
    return 0;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_tree_new(scl_ml_tree_t **model, scl_ml_tree_params_t params) {
    if (scl_unlikely(!model)) return SCL_ERR_NULL_PTR;
    scl_ml_tree_t *t = (scl_ml_tree_t *)calloc(1, sizeof(scl_ml_tree_t));
    if (scl_unlikely(!t)) return SCL_ERR_OUT_OF_MEMORY;
    t->params = params;
    *model = t;
    return SCL_OK;
}

void
scl_ml_tree_free(scl_ml_tree_t *model) {
    if (scl_unlikely(!model)) return;
    free(model->nodes);
    memset(model, 0, sizeof(*model));
    free(model);
}

SCL_WARN_UNUSED scl_error_t
scl_ml_tree_fit(scl_ml_tree_t *model, const scl_ml_dataset_t *ds) {
    if (scl_unlikely(!model || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!ds->data || ds->n_rows == 0)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(scl_ml_dataset_has_missing(ds))) return SCL_ERR_ML_MISSING_DATA;

    scl_ml_simd_init();

    scl_ml_tree_t *t = model;
    t->n_features = ds->n_cols;
    t->n_nodes = 0;
    t->n_leaves = 0;
    t->fitted = 0;

    SCL_ML_FLOAT min_target = ds->targets[0];
    SCL_ML_FLOAT max_target = ds->targets[0];
    for (size_t i = 1; i < ds->n_rows; i++) {
        if (ds->targets[i] < min_target) min_target = ds->targets[i];
        if (ds->targets[i] > max_target) max_target = ds->targets[i];
    }

    double min_d = (double)min_target;
    double max_d = (double)max_target;
    double range = max_d - min_d;

    t->is_classifier = 0;
    t->n_classes = 0;

    if (t->params.criterion == SCL_ML_CRITERION_GINI ||
        t->params.criterion == SCL_ML_CRITERION_ENTROPY) {
        t->is_classifier = 1;
        t->n_classes = (size_t)(max_d - min_d + 1);
        if (t->n_classes < 2) t->n_classes = 2;
    }

    if (range < 0.5 && min_d >= 0.0 && t->is_classifier == 0) {
        int int_range = (int)(max_d - min_d + 1);
        if (int_range <= 100 && int_range >= 2) {
            t->is_classifier = 1;
            t->n_classes = (size_t)int_range;
        }
    }

    size_t *indices = (size_t *)malloc(ds->n_rows * sizeof(size_t));
    if (!indices) return SCL_ERR_OUT_OF_MEMORY;
    for (size_t i = 0; i < ds->n_rows; i++)
        indices[i] = i;

    size_t n_features_use = t->params.max_features > 0 ?
        t->params.max_features : t->n_features;
    if (n_features_use > t->n_features) n_features_use = t->n_features;

    size_t *feature_subset = (size_t *)malloc(
        n_features_use * sizeof(size_t));
    if (!feature_subset) { free(indices); return SCL_ERR_OUT_OF_MEMORY; }
    for (size_t i = 0; i < n_features_use; i++)
        feature_subset[i] = i;

    int ret = scl_ml_tree_grow(t, ds, indices, ds->n_rows, 0,
                                feature_subset, n_features_use);

    free(feature_subset);
    free(indices);

    if (ret != 0) {
        free(t->nodes);
        t->nodes = NULL;
        t->n_nodes = 0;
        return SCL_ERR_OUT_OF_MEMORY;
    }

    t->fitted = 1;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_tree_predict(scl_ml_tree_t *model, const scl_ml_dataset_t *ds,
                     SCL_ML_FLOAT *y_out) {
    if (scl_unlikely(!model || !ds || !y_out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(ds->n_cols != model->n_features)) return SCL_ERR_INVALID_ARG;

    for (size_t i = 0; i < ds->n_rows; i++) {
        int node_idx = 0;
        while (1) {
            scl_ml_tree_node_t *n = &model->nodes[node_idx];
            if (n->feature_idx < 0) {
                y_out[i] = n->value;
                break;
            }
            if (ds->data[i * ds->row_stride + (size_t)n->feature_idx] <= n->threshold)
                node_idx = n->left_child;
            else
                node_idx = n->right_child;
        }
    }

    return SCL_OK;
}

SCL_PURE size_t
scl_ml_tree_get_n_features(const scl_ml_tree_t *model) {
    return model ? model->n_features : 0;
}

SCL_PURE size_t
scl_ml_tree_get_n_nodes(const scl_ml_tree_t *model) {
    return model ? model->n_nodes : 0;
}

SCL_PURE size_t
scl_ml_tree_get_n_leaves(const scl_ml_tree_t *model) {
    return model ? model->n_leaves : 0;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_tree_save(const scl_ml_tree_t *model,
                  uint8_t **buf, size_t *len, scl_allocator_t *alloc) {
    (void)alloc;
    if (scl_unlikely(!model || !buf || !len)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;

    size_t nodes_bytes = model->n_nodes * sizeof(scl_ml_tree_node_t);

    scl_ml_serial_header_t hdr;
    hdr.magic     = SCL_ML_MAGIC;
    hdr.version   = SCL_ML_FORMAT_VERSION;
    hdr.algo_id   = SCL_ML_ALGO_TREE;
    hdr.header_sz = sizeof(hdr);
    hdr.reserved  = 0;

    size_t payload_sz = sizeof(size_t) * 3 + sizeof(int) + nodes_bytes;
    size_t total = sizeof(hdr) + payload_sz + sizeof(uint32_t);

    uint8_t *buffer = (uint8_t *)calloc(1, total);
    if (!buffer) return SCL_ERR_OUT_OF_MEMORY;

    memcpy(buffer, &hdr, sizeof(hdr));
    size_t off = sizeof(hdr);

    memcpy(buffer + off, &model->n_nodes, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->n_leaves, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->n_features, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->is_classifier, sizeof(int)); off += sizeof(int);
    memcpy(buffer + off, model->nodes, nodes_bytes); off += nodes_bytes;

    uint32_t crc = 0;
    memcpy(buffer + off, &crc, sizeof(crc));

    *buf = buffer;
    *len = total;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_tree_load(scl_ml_tree_t **model,
                  const uint8_t *buf, size_t len,
                  scl_ml_tree_params_t params) {
    if (scl_unlikely(!model || !buf)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(len < sizeof(scl_ml_serial_header_t) + sizeof(uint32_t)))
        return SCL_ERR_INVALID_ARG;

    const scl_ml_serial_header_t *hdr = (const scl_ml_serial_header_t *)buf;
    if (scl_unlikely(hdr->magic != SCL_ML_MAGIC ||
                     hdr->algo_id != SCL_ML_ALGO_TREE))
        return SCL_ERR_INVALID_ARG;

    scl_ml_tree_t *t;
    scl_error_t err = scl_ml_tree_new(&t, params);
    if (err != SCL_OK) return err;

    size_t off = sizeof(*hdr);

    memcpy(&t->n_nodes, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&t->n_leaves, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&t->n_features, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&t->is_classifier, buf + off, sizeof(int)); off += sizeof(int);

    t->n_capacity = t->n_nodes;
    size_t nodes_bytes = t->n_nodes * sizeof(scl_ml_tree_node_t);
    t->nodes = (scl_ml_tree_node_t *)calloc(t->n_capacity,
                                             sizeof(scl_ml_tree_node_t));
    if (!t->nodes) { free(t); return SCL_ERR_OUT_OF_MEMORY; }
    memcpy(t->nodes, buf + off, nodes_bytes);

    t->fitted = 1;
    *model = t;
    return SCL_OK;
}
