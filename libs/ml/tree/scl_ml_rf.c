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

#include "scl_ml_rf.h"
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
} scl_ml_rf_node_t;

typedef struct {
    scl_ml_rf_node_t *nodes;
    size_t            n_nodes;
    size_t            n_leaves;
    size_t            n_features;
    int               is_classifier;
} scl_ml_rf_internal_tree_t;

typedef struct scl_ml_rf {
    scl_ml_rf_params_t        params;
    scl_ml_rf_internal_tree_t *trees;
    size_t                    n_estimators;
    size_t                    n_features;
    size_t                    n_classes;
    int                       fitted;
    int                       is_classifier;
} scl_ml_rf_t;

static uint32_t
scl_ml_rf_rand(uint32_t *state) {
    *state = *state * 1103515245u + 12345u;
    return *state;
}

static double
scl_ml_rf_rand_uniform(uint32_t *state) {
    return (double)scl_ml_rf_rand(state) / 4294967296.0;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_rf_new(scl_ml_rf_t **model, scl_ml_rf_params_t params) {
    if (scl_unlikely(!model)) return SCL_ERR_NULL_PTR;
    scl_ml_rf_t *m = (scl_ml_rf_t *)calloc(1, sizeof(scl_ml_rf_t));
    if (scl_unlikely(!m)) return SCL_ERR_OUT_OF_MEMORY;
    m->params = params;
    *model = m;
    return SCL_OK;
}

void
scl_ml_rf_free(scl_ml_rf_t *model) {
    if (scl_unlikely(!model)) return;
    for (size_t i = 0; i < model->n_estimators; i++)
        free(model->trees[i].nodes);
    free(model->trees);
    memset(model, 0, sizeof(*model));
    free(model);
}

static int
scl_ml_rf_train_single_tree(
    scl_ml_rf_internal_tree_t *tree_out,
    const scl_ml_dataset_t *ds,
    uint32_t *rng_state,
    size_t n_features_total,
    int is_classifier,
    scl_ml_rf_params_t *params) {

    size_t n = ds->n_rows;
    size_t *boot_idx = (size_t *)malloc(n * sizeof(size_t));
    if (!boot_idx) return -1;

    for (size_t i = 0; i < n; i++)
        boot_idx[i] = (size_t)(scl_ml_rf_rand_uniform(rng_state) * (double)n);
    for (size_t i = 0; i < n; i++)
        if (boot_idx[i] >= n) boot_idx[i] = n - 1;

    SCL_ML_FLOAT *boot_data = (SCL_ML_FLOAT *)malloc(
        n * n_features_total * sizeof(SCL_ML_FLOAT));
    SCL_ML_FLOAT *boot_targets = (SCL_ML_FLOAT *)malloc(
        n * sizeof(SCL_ML_FLOAT));
    if (!boot_data || !boot_targets) {
        free(boot_idx); free(boot_data); free(boot_targets);
        return -1;
    }

    for (size_t i = 0; i < n; i++) {
        size_t src = boot_idx[i];
        for (size_t j = 0; j < n_features_total; j++)
            boot_data[i * n_features_total + j] =
                ds->data[src * ds->row_stride + j];
        boot_targets[i] = ds->targets[src];
    }

    scl_ml_dataset_t boot_ds;
    memset(&boot_ds, 0, sizeof(boot_ds));
    boot_ds.data = boot_data;
    boot_ds.targets = boot_targets;
    boot_ds.n_rows = n;
    boot_ds.n_cols = n_features_total;
    boot_ds.row_stride = n_features_total;

    scl_ml_tree_params_t tree_params;
    memset(&tree_params, 0, sizeof(tree_params));
    tree_params.max_depth = params->max_depth;
    tree_params.min_samples_split = params->min_samples_split;
    tree_params.min_samples_leaf = params->min_samples_leaf;
    tree_params.criterion = params->criterion;
    tree_params.min_impurity_decrease = 0.0;

    size_t max_f = params->max_features;
    if (max_f == 0) {
        if (is_classifier)
            max_f = (size_t)sqrt((double)n_features_total);
        else
            max_f = n_features_total / 3;
        if (max_f < 1) max_f = 1;
    }
    if (max_f > n_features_total) max_f = n_features_total;
    tree_params.max_features = max_f;

    scl_ml_tree_t *t = NULL;
    scl_error_t err = scl_ml_tree_new(&t, tree_params);
    if (err != SCL_OK) {
        free(boot_idx); free(boot_data); free(boot_targets);
        return -1;
    }

    err = scl_ml_tree_fit(t, &boot_ds);
    if (err != SCL_OK) {
        scl_ml_tree_free(t);
        free(boot_idx); free(boot_data); free(boot_targets);
        return -1;
    }

    tree_out->n_nodes = scl_ml_tree_get_n_nodes(t);
    tree_out->n_leaves = 0;
    tree_out->n_features = n_features_total;
    tree_out->is_classifier = is_classifier;

    tree_out->nodes = (scl_ml_rf_node_t *)malloc(
        tree_out->n_nodes * sizeof(scl_ml_rf_node_t));
    if (!tree_out->nodes) {
        scl_ml_tree_free(t);
        free(boot_idx); free(boot_data); free(boot_targets);
        return -1;
    }

    uint8_t *buf = NULL;
    size_t buflen = 0;
    err = scl_ml_tree_save(t, &buf, &buflen, NULL);
    if (err != SCL_OK) {
        free(tree_out->nodes);
        scl_ml_tree_free(t);
        free(boot_idx); free(boot_data); free(boot_targets);
        return -1;
    }

    size_t off = sizeof(scl_ml_serial_header_t);
    memcpy(&tree_out->n_nodes, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&tree_out->n_leaves, buf + off, sizeof(size_t)); off += sizeof(size_t);
    off += sizeof(size_t);
    int isc;
    memcpy(&isc, buf + off, sizeof(int));
    tree_out->is_classifier = isc;
    off += sizeof(int);

    memcpy(tree_out->nodes, buf + off,
           tree_out->n_nodes * sizeof(scl_ml_rf_node_t));

    free(buf);
    scl_ml_tree_free(t);
    free(boot_idx); free(boot_data); free(boot_targets);
    return 0;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_rf_fit(scl_ml_rf_t *model, const scl_ml_dataset_t *ds) {
    if (scl_unlikely(!model || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!ds->data || ds->n_rows == 0)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(scl_ml_dataset_has_missing(ds))) return SCL_ERR_ML_MISSING_DATA;

    scl_ml_simd_init();

    model->n_features = ds->n_cols;
    model->fitted = 0;
    model->n_estimators = model->params.n_estimators;

    SCL_ML_FLOAT min_target = ds->targets[0];
    SCL_ML_FLOAT max_target = ds->targets[0];
    for (size_t i = 1; i < ds->n_rows; i++) {
        if (ds->targets[i] < min_target) min_target = ds->targets[i];
        if (ds->targets[i] > max_target) max_target = ds->targets[i];
    }

    double min_d = (double)min_target;
    double max_d = (double)max_target;
    double range = max_d - min_d;

    model->is_classifier = 0;
    model->n_classes = 0;

    if (model->params.criterion == SCL_ML_CRITERION_GINI ||
        model->params.criterion == SCL_ML_CRITERION_ENTROPY) {
        model->is_classifier = 1;
        model->n_classes = (size_t)(max_d - min_d + 1);
        if (model->n_classes < 2) model->n_classes = 2;
    }

    if (range < 0.5 && min_d >= 0.0 && !model->is_classifier) {
        int int_range = (int)(max_d - min_d + 1);
        if (int_range <= 100 && int_range >= 2) {
            model->is_classifier = 1;
            model->n_classes = (size_t)int_range;
        }
    }

    uint32_t seed = (uint32_t)model->params.random_seed;
    if (seed == (uint32_t)-1) seed = 42;

    model->trees = (scl_ml_rf_internal_tree_t *)calloc(
        model->n_estimators, sizeof(scl_ml_rf_internal_tree_t));
    if (!model->trees) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t i = 0; i < model->n_estimators; i++) {
        seed = seed * 1103515245u + 12345u;
        uint32_t local_seed = seed;
        int ret = scl_ml_rf_train_single_tree(
            &model->trees[i], ds, &local_seed,
            model->n_features,
            model->is_classifier,
            &model->params);
        if (ret != 0) {
            for (size_t j = 0; j <= i; j++)
                free(model->trees[j].nodes);
            free(model->trees);
            model->trees = NULL;
            return SCL_ERR_OUT_OF_MEMORY;
        }
    }

    model->fitted = 1;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_rf_predict(scl_ml_rf_t *model, const scl_ml_dataset_t *ds,
                   SCL_ML_FLOAT *y_out) {
    if (scl_unlikely(!model || !ds || !y_out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(ds->n_cols != model->n_features)) return SCL_ERR_INVALID_ARG;

    if (model->is_classifier) {
        size_t *votes = (size_t *)calloc(model->n_classes, sizeof(size_t));
        if (!votes) return SCL_ERR_OUT_OF_MEMORY;

        for (size_t i = 0; i < ds->n_rows; i++) {
            memset(votes, 0, model->n_classes * sizeof(size_t));

            for (size_t t = 0; t < model->n_estimators; t++) {
                scl_ml_rf_internal_tree_t *tr = &model->trees[t];
                int node_idx = 0;
                while (1) {
                    scl_ml_rf_node_t *n = &tr->nodes[node_idx];
                    if (n->feature_idx < 0) {
                        int cls = (int)n->value;
                        if (cls >= 0 && (size_t)cls < model->n_classes)
                            votes[cls]++;
                        break;
                    }
                    if (ds->data[i * ds->row_stride +
                                 (size_t)n->feature_idx] <= n->threshold)
                        node_idx = n->left_child;
                    else
                        node_idx = n->right_child;
                }
            }

            size_t best_cls = 0;
            size_t best_votes = votes[0];
            for (size_t k = 1; k < model->n_classes; k++) {
                if (votes[k] > best_votes) {
                    best_votes = votes[k];
                    best_cls = k;
                }
            }
            y_out[i] = (SCL_ML_FLOAT)best_cls;
        }

        free(votes);
    } else {
        for (size_t i = 0; i < ds->n_rows; i++) {
            double sum = 0.0;
            for (size_t t = 0; t < model->n_estimators; t++) {
                scl_ml_rf_internal_tree_t *tr = &model->trees[t];
                int node_idx = 0;
                while (1) {
                    scl_ml_rf_node_t *n = &tr->nodes[node_idx];
                    if (n->feature_idx < 0) {
                        sum += (double)n->value;
                        break;
                    }
                    if (ds->data[i * ds->row_stride +
                                 (size_t)n->feature_idx] <= n->threshold)
                        node_idx = n->left_child;
                    else
                        node_idx = n->right_child;
                }
            }
            y_out[i] = (SCL_ML_FLOAT)(sum / (double)model->n_estimators);
        }
    }

    return SCL_OK;
}

SCL_PURE size_t
scl_ml_rf_get_n_features(const scl_ml_rf_t *model) {
    return model ? model->n_features : 0;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_rf_save(const scl_ml_rf_t *model,
                uint8_t **buf, size_t *len, scl_allocator_t *alloc) {
    (void)alloc;
    if (scl_unlikely(!model || !buf || !len)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;

    size_t trees_data_sz = 0;
    for (size_t i = 0; i < model->n_estimators; i++) {
        trees_data_sz += sizeof(size_t) * 2 + sizeof(int) +
                         model->trees[i].n_nodes * sizeof(scl_ml_rf_node_t);
    }

    scl_ml_serial_header_t hdr;
    hdr.magic     = SCL_ML_MAGIC;
    hdr.version   = SCL_ML_FORMAT_VERSION;
    hdr.algo_id   = SCL_ML_ALGO_RF;
    hdr.header_sz = sizeof(hdr);
    hdr.reserved  = 0;

    size_t payload_sz = sizeof(size_t) * 3 + sizeof(int) + trees_data_sz;
    size_t total = sizeof(hdr) + payload_sz + sizeof(uint32_t);

    uint8_t *buffer = (uint8_t *)calloc(1, total);
    if (!buffer) return SCL_ERR_OUT_OF_MEMORY;

    memcpy(buffer, &hdr, sizeof(hdr));
    size_t off = sizeof(hdr);

    memcpy(buffer + off, &model->n_estimators, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->n_features, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->n_classes, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->is_classifier, sizeof(int)); off += sizeof(int);

    for (size_t i = 0; i < model->n_estimators; i++) {
        memcpy(buffer + off, &model->trees[i].n_nodes, sizeof(size_t));
        off += sizeof(size_t);
        memcpy(buffer + off, &model->trees[i].n_leaves, sizeof(size_t));
        off += sizeof(size_t);
        memcpy(buffer + off, &model->trees[i].is_classifier, sizeof(int));
        off += sizeof(int);
        size_t nb = model->trees[i].n_nodes * sizeof(scl_ml_rf_node_t);
        memcpy(buffer + off, model->trees[i].nodes, nb);
        off += nb;
    }

    uint32_t crc = 0;
    memcpy(buffer + off, &crc, sizeof(crc));

    *buf = buffer;
    *len = total;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_rf_load(scl_ml_rf_t **model,
                const uint8_t *buf, size_t len,
                scl_ml_rf_params_t params) {
    if (scl_unlikely(!model || !buf)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(len < sizeof(scl_ml_serial_header_t) + sizeof(uint32_t)))
        return SCL_ERR_INVALID_ARG;

    const scl_ml_serial_header_t *hdr = (const scl_ml_serial_header_t *)buf;
    if (scl_unlikely(hdr->magic != SCL_ML_MAGIC ||
                     hdr->algo_id != SCL_ML_ALGO_RF))
        return SCL_ERR_INVALID_ARG;

    scl_ml_rf_t *m;
    scl_error_t err = scl_ml_rf_new(&m, params);
    if (err != SCL_OK) return err;

    size_t off = sizeof(*hdr);

    memcpy(&m->n_estimators, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&m->n_features, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&m->n_classes, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&m->is_classifier, buf + off, sizeof(int)); off += sizeof(int);

    m->trees = (scl_ml_rf_internal_tree_t *)calloc(
        m->n_estimators, sizeof(scl_ml_rf_internal_tree_t));
    if (!m->trees) { free(m); return SCL_ERR_OUT_OF_MEMORY; }

    for (size_t i = 0; i < m->n_estimators; i++) {
        memcpy(&m->trees[i].n_nodes, buf + off, sizeof(size_t));
        off += sizeof(size_t);
        memcpy(&m->trees[i].n_leaves, buf + off, sizeof(size_t));
        off += sizeof(size_t);
        memcpy(&m->trees[i].is_classifier, buf + off, sizeof(int));
        off += sizeof(int);

        size_t nb = m->trees[i].n_nodes * sizeof(scl_ml_rf_node_t);
        m->trees[i].nodes = (scl_ml_rf_node_t *)calloc(
            m->trees[i].n_nodes, sizeof(scl_ml_rf_node_t));
        if (!m->trees[i].nodes) {
            for (size_t j = 0; j < i; j++)
                free(m->trees[j].nodes);
            free(m->trees); free(m);
            return SCL_ERR_OUT_OF_MEMORY;
        }
        memcpy(m->trees[i].nodes, buf + off, nb);
        off += nb;
    }

    m->fitted = 1;
    *model = m;
    return SCL_OK;
}
