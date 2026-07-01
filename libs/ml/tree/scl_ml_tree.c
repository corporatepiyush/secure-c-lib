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
#include "scl_alloc_arena.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

/* Node type is defined in the header (exposed for ensemble zero-copy handoff). */

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
    scl_allocator_t     *alloc;
    scl_allocator_t     *scratch;
} scl_ml_tree_t;

static const size_t SCL_TREE_INIT_CAPACITY = 64;

static int
scl_ml_tree_ensure_capacity(scl_ml_tree_t *tree, size_t needed) {
    if (needed <= tree->n_capacity) return 0;
    size_t new_cap = tree->n_capacity ? tree->n_capacity * 2 : SCL_TREE_INIT_CAPACITY;
    while (new_cap < needed) new_cap *= 2;
    scl_ml_tree_node_t *p = (scl_ml_tree_node_t *)scl_realloc(
        tree->alloc, tree->nodes,
        tree->n_capacity * sizeof(scl_ml_tree_node_t),
        new_cap * sizeof(scl_ml_tree_node_t),
        alignof(max_align_t));
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
                     size_t n_classes, scl_allocator_t *a) {
    if (n == 0) return 0.0f;
    size_t *counts = (size_t *)scl_calloc(a, n_classes, sizeof(size_t), alignof(max_align_t));
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
    scl_free(a, counts);
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
                         int is_classifier, size_t n_classes,
                         scl_allocator_t *a) {
    if (is_classifier)
        return scl_ml_compute_gini(targets, indices, n, n_classes, a);
    return scl_ml_compute_mse(targets, indices, n);
}

static SCL_ML_FLOAT
scl_ml_compute_leaf_value(const SCL_ML_FLOAT *targets,
                           const size_t *indices, size_t n,
                           int is_classifier, size_t n_classes,
                           scl_allocator_t *a) {
    if (!is_classifier) {
        if (n == 0) return 0.0f;
        double sum = 0.0;
        for (size_t i = 0; i < n; i++)
            sum += (double)targets[indices[i]];
        return (SCL_ML_FLOAT)(sum / (double)n);
    }
    if (n == 0) return 0.0f;
    size_t *counts = (size_t *)scl_calloc(a, n_classes, sizeof(size_t), alignof(max_align_t));
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
    scl_free(a, counts);
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
    SCL_ML_FLOAT *best_impurity_reduction,
    scl_allocator_t *a) {

    *best_feature = -1;
    *best_threshold = 0.0f;
    *best_impurity_reduction = 0.0f;
    if (n <= 1) return 0;

    double best_reduction = -1.0;

    /* Sort buffer: allocated ONCE for the whole call, reused across features. */
    scl_ml_sort_pair_t *pairs =
        (scl_ml_sort_pair_t *)scl_alloc(a, n * sizeof(scl_ml_sort_pair_t), alignof(max_align_t));
    if (!pairs) return -1;

    /* Incremental-impurity scratch (one alloc per call, not per candidate).
     * Classification: running class counts + sum-of-c^2.
     * Regression: prefix sums of target and target^2 for O(1) split variance. */
    size_t *left_counts = NULL, *total_counts = NULL;
    double *prefix_sum = NULL, *prefix_sqsum = NULL;

    if (is_classifier) {
        left_counts  = (size_t *)scl_calloc(a, n_classes, sizeof(size_t), alignof(max_align_t));
        total_counts = (size_t *)scl_calloc(a, n_classes, sizeof(size_t), alignof(max_align_t));
        if (!left_counts || !total_counts) {
            scl_free(a, pairs); scl_free(a, left_counts); scl_free(a, total_counts);
            return -1;
        }
    } else {
        prefix_sum   = (double *)scl_alloc(a, (n + 1) * sizeof(double), alignof(max_align_t));
        prefix_sqsum = (double *)scl_alloc(a, (n + 1) * sizeof(double), alignof(max_align_t));
        if (!prefix_sum || !prefix_sqsum) {
            scl_free(a, pairs); scl_free(a, prefix_sum); scl_free(a, prefix_sqsum);
            return -1;
        }
    }

    for (size_t f = 0; f < n_subset; f++) {
        size_t feat = feature_subset[f];

        for (size_t i = 0; i < n; i++) {
            size_t row = indices[i];
            pairs[i].value    = ds->data[row * ds->row_stride + feat];
            pairs[i].target   = ds->targets[row];
            pairs[i].orig_idx = indices[i];
        }

        qsort(pairs, n, sizeof(scl_ml_sort_pair_t), scl_ml_sort_pair_cmp);

        double best_feat_reduction = -1.0;
        SCL_ML_FLOAT best_feat_threshold = 0.0f;

        if (is_classifier) {
            /* Total class counts + total sum-of-c² for this node. */
            memset(total_counts, 0, n_classes * sizeof(size_t));
            for (size_t i = 0; i < n; i++) {
                int cls = (int)pairs[i].target;
                if (cls >= 0 && (size_t)cls < n_classes) total_counts[cls]++;
            }
            double total_c2 = 0.0;
            for (size_t c = 0; c < n_classes; c++)
                total_c2 += (double)total_counts[c] * (double)total_counts[c];

            /* Maintain left & right partition counts simultaneously.
             * sum_c2_{left,right} track Σ c_k² over each partition so gini
             * = 1 - Σc² / n².  NOTE: Σ(total-left)² ≠ Σtotal² - Σleft², so
             * the right side must be maintained independently (decremental). */
            memset(left_counts, 0, n_classes * sizeof(size_t));
            size_t *right_counts = total_counts;   /* alias: starts as totals */
            size_t n_left = 0;
            double sum_c2_left  = 0.0;
            double sum_c2_right = total_c2;

            for (size_t split_i = 0; split_i < n - 1; split_i++) {
                int cls = (int)pairs[split_i].target;
                if (cls >= 0 && (size_t)cls < n_classes) {
                    size_t l_before = left_counts[cls];
                    size_t r_before = right_counts[cls];   /* == total - l_before */
                    left_counts[cls]  = l_before + 1;
                    right_counts[cls] = r_before - 1;
                    /* (l+1)² - l² = 2l + 1 ;  r² - (r-1)² = 2r - 1 */
                    sum_c2_left  += 2.0 * (double)l_before + 1.0;
                    sum_c2_right -= 2.0 * (double)r_before - 1.0;
                }
                n_left++;

                if (pairs[split_i].value == pairs[split_i + 1].value)
                    continue;
                size_t n_right = n - n_left;
                if (n_left < min_samples_leaf || n_right < min_samples_leaf)
                    continue;

                double left_gini  = 1.0 - sum_c2_left  / ((double)n_left  * (double)n_left);
                double right_gini = 1.0 - sum_c2_right / ((double)n_right * (double)n_right);

                double reduction = (double)current_impurity -
                    ((double)n_left / (double)n) * left_gini -
                    ((double)n_right / (double)n) * right_gini;

                if (reduction > best_feat_reduction) {
                    best_feat_reduction = reduction;
                    best_feat_threshold = (pairs[split_i].value +
                                           pairs[split_i + 1].value) / 2.0f;
                }
            }
        } else {
            /* Regression: prefix sums → O(1) MSE per split candidate. */
            prefix_sum[0] = 0.0;
            prefix_sqsum[0] = 0.0;
            for (size_t i = 0; i < n; i++) {
                double t = (double)pairs[i].target;
                prefix_sum[i + 1]   = prefix_sum[i]   + t;
                prefix_sqsum[i + 1] = prefix_sqsum[i] + t * t;
            }
            double total_sum = prefix_sum[n];
            double total_sq  = prefix_sqsum[n];

            for (size_t split_i = 0; split_i < n - 1; split_i++) {
                if (pairs[split_i].value == pairs[split_i + 1].value)
                    continue;
                size_t n_left = split_i + 1;
                size_t n_right = n - n_left;
                if (n_left < min_samples_leaf || n_right < min_samples_leaf)
                    continue;

                double l_sum = prefix_sum[n_left];
                double l_sq  = prefix_sqsum[n_left];
                double l_mean = l_sum / (double)n_left;
                double l_var  = l_sq - l_mean * l_sum;   /* sum (x-mean)^2 */
                double l_mse  = l_var / (double)n_left;

                double r_sum = total_sum - l_sum;
                double r_sq  = total_sq  - l_sq;
                double r_mean = r_sum / (double)n_right;
                double r_var  = r_sq - r_mean * r_sum;
                double r_mse  = r_var / (double)n_right;

                double reduction = (double)current_impurity -
                    ((double)n_left / (double)n) * l_mse -
                    ((double)n_right / (double)n) * r_mse;

                if (reduction > best_feat_reduction) {
                    best_feat_reduction = reduction;
                    best_feat_threshold = (pairs[split_i].value +
                                           pairs[split_i + 1].value) / 2.0f;
                }
            }
        }

        if (best_feat_reduction > best_reduction) {
            best_reduction = best_feat_reduction;
            *best_feature = (int)feat;
            *best_threshold = best_feat_threshold;
            *best_impurity_reduction = (SCL_ML_FLOAT)best_feat_reduction;
        }
    }

    scl_free(a, pairs);
    scl_free(a, left_counts);
    scl_free(a, total_counts);
    scl_free(a, prefix_sum);
    scl_free(a, prefix_sqsum);

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
        ds->targets, indices, n, tree->is_classifier, tree->n_classes, tree->scratch);

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
            ds->targets, indices, n, tree->is_classifier, tree->n_classes, tree->scratch);
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
        &best_feature, &best_threshold, &best_reduction,
        tree->scratch);

    if (ret != 0) return -1;

    if (best_feature < 0) {
        node->value = scl_ml_compute_leaf_value(
            ds->targets, indices, n, tree->is_classifier, tree->n_classes, tree->scratch);
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

    size_t *left_idx = (size_t *)scl_alloc(tree->scratch, n_left * sizeof(size_t), alignof(max_align_t));
    size_t *right_idx = (size_t *)scl_alloc(tree->scratch, n_right * sizeof(size_t), alignof(max_align_t));
    if (!left_idx || !right_idx) {
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
        return -1;
    }
    node = &tree->nodes[node_idx];
    node->left_child = left_child;

    int right_child = (int)tree->n_nodes;
    if (scl_ml_tree_grow(tree, ds, right_idx, n_right, depth + 1,
                          feature_subset, n_subset) != 0) {
        return -1;
    }
    node = &tree->nodes[node_idx];
    node->right_child = right_child;

    return 0;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_tree_new(scl_ml_tree_t **model, scl_ml_tree_params_t params) {
    if (scl_unlikely(!model)) return SCL_ERR_NULL_PTR;
    if (!params.alloc) return SCL_ERR_INVALID_ARG;
    scl_allocator_t *alloc = params.alloc;
    scl_ml_tree_t *t = (scl_ml_tree_t *)scl_calloc(
        alloc, 1, sizeof(scl_ml_tree_t), alignof(max_align_t));
    if (scl_unlikely(!t)) return SCL_ERR_OUT_OF_MEMORY;
    t->params = params;
    t->alloc  = alloc;
    t->scratch = scl_alloc_arena_create(alloc, 8192, 0);
    if (!t->scratch) { scl_free(alloc, t); return SCL_ERR_OUT_OF_MEMORY; }
    *model = t;
    return SCL_OK;
}

void
scl_ml_tree_free(scl_ml_tree_t *model) {
    if (scl_unlikely(!model)) return;
    scl_allocator_t *a = model->alloc;
    scl_free(a, model->nodes);
    if (model->scratch) scl_alloc_arena_destroy(model->scratch);
    memset(model, 0, sizeof(*model));
    scl_free(a, model);
}

SCL_WARN_UNUSED scl_error_t
scl_ml_tree_fit(scl_ml_tree_t *model, const scl_ml_dataset_t *ds) {
    if (scl_unlikely(!model || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!ds->data || ds->n_rows == 0)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(scl_ml_dataset_has_missing(ds))) return SCL_ERR_ML_MISSING_DATA;

    scl_ml_simd_init();

    scl_ml_tree_t *t = model;
    scl_alloc_arena_reset(t->scratch);
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

    size_t *indices = (size_t *)scl_alloc(t->scratch, ds->n_rows * sizeof(size_t), alignof(max_align_t));
    if (!indices) return SCL_ERR_OUT_OF_MEMORY;
    for (size_t i = 0; i < ds->n_rows; i++)
        indices[i] = i;

    size_t n_features_use = t->params.max_features > 0 ?
        t->params.max_features : t->n_features;
    if (n_features_use > t->n_features) n_features_use = t->n_features;

    size_t *feature_subset = (size_t *)scl_alloc(
        t->scratch, n_features_use * sizeof(size_t), alignof(max_align_t));
    if (!feature_subset) return SCL_ERR_OUT_OF_MEMORY;
    for (size_t i = 0; i < n_features_use; i++)
        feature_subset[i] = i;

    int ret = scl_ml_tree_grow(t, ds, indices, ds->n_rows, 0,
                                feature_subset, n_features_use);

    if (ret != 0) {
        scl_free(t->alloc, t->nodes);
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

SCL_PURE const scl_ml_tree_node_t *
scl_ml_tree_get_nodes(const scl_ml_tree_t *model) {
    return (model && model->fitted) ? model->nodes : NULL;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_tree_save(const scl_ml_tree_t *model,
                  uint8_t **buf, size_t *len, scl_allocator_t *alloc) {
    if (scl_unlikely(!model || !buf || !len)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(!alloc)) return SCL_ERR_NULL_PTR;

    size_t nodes_bytes = model->n_nodes * sizeof(scl_ml_tree_node_t);

    scl_ml_serial_header_t hdr;
    hdr.magic     = SCL_ML_MAGIC;
    hdr.version   = SCL_ML_FORMAT_VERSION;
    hdr.algo_id   = SCL_ML_ALGO_TREE;
    hdr.header_sz = sizeof(hdr);
    hdr.reserved  = 0;

    size_t payload_sz = sizeof(size_t) * 3 + sizeof(int) + nodes_bytes;
    size_t total = sizeof(hdr) + payload_sz + sizeof(uint32_t);

    uint8_t *buffer = (uint8_t *)scl_calloc(alloc, 1, total, alignof(max_align_t));
    if (!buffer) return SCL_ERR_OUT_OF_MEMORY;

    memcpy(buffer, &hdr, sizeof(hdr));
    size_t off = sizeof(hdr);

    memcpy(buffer + off, &model->n_nodes, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->n_leaves, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->n_features, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->is_classifier, sizeof(int)); off += sizeof(int);
    memcpy(buffer + off, model->nodes, nodes_bytes); off += nodes_bytes;

    uint32_t crc = scl_ml_crc32c(buffer + sizeof(scl_ml_serial_header_t), payload_sz);
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

    /* Verify payload integrity before any allocation/parsing. */
    uint32_t stored_crc = 0;
    memcpy(&stored_crc, buf + len - sizeof(uint32_t), sizeof(uint32_t));
    uint32_t expected_crc = scl_ml_crc32c(
        buf + sizeof(scl_ml_serial_header_t),
        len - sizeof(scl_ml_serial_header_t) - sizeof(uint32_t));
    if (scl_unlikely(stored_crc != expected_crc))
        return SCL_ERR_INVALID_ARG;

    size_t off = sizeof(*hdr);

    memcpy(&t->n_nodes, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&t->n_leaves, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&t->n_features, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&t->is_classifier, buf + off, sizeof(int)); off += sizeof(int);

    t->n_capacity = t->n_nodes;
    size_t nodes_bytes = t->n_nodes * sizeof(scl_ml_tree_node_t);
    t->nodes = (scl_ml_tree_node_t *)scl_calloc(t->alloc, t->n_capacity,
                                                  sizeof(scl_ml_tree_node_t),
                                                  alignof(max_align_t));
    if (!t->nodes) { scl_ml_tree_free(t); return SCL_ERR_OUT_OF_MEMORY; }
    memcpy(t->nodes, buf + off, nodes_bytes);

    t->fitted = 1;
    *model = t;
    return SCL_OK;
}
