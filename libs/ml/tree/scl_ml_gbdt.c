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

#include "scl_ml_gbdt.h"
#include "scl_ml_tree.h"
#include "scl_ml_simd.h"
#include "scl_alloc_arena.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

/* GBDT stores its own copy of each regression tree's nodes; layout is
 * identical to scl_ml_tree_node_t — alias and lift via one memcpy. */
typedef scl_ml_tree_node_t scl_ml_gbdt_node_t;

typedef struct {
    scl_ml_gbdt_node_t *nodes;
    size_t              n_nodes;
    size_t              n_leaves;
    size_t              n_features;
    int                 is_classifier;
} scl_ml_gbdt_internal_tree_t;

typedef struct scl_ml_gbdt {
    scl_ml_gbdt_params_t        params;
    scl_ml_gbdt_internal_tree_t *trees;
    SCL_ML_FLOAT                base_score;
    size_t                      n_estimators;
    size_t                      n_features;
    int                         fitted;
    scl_allocator_t            *alloc;
    scl_allocator_t            *scratch;
} scl_ml_gbdt_t;

static uint32_t
scl_ml_gbdt_rand(uint32_t *state) {
    *state = *state * 1103515245u + 12345u;
    return *state;
}

static double
scl_ml_gbdt_rand_uniform(uint32_t *state) {
    return (double)scl_ml_gbdt_rand(state) / 4294967296.0;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_gbdt_new(scl_ml_gbdt_t **model, scl_ml_gbdt_params_t params) {
    if (scl_unlikely(!model)) return SCL_ERR_NULL_PTR;
    if (!params.alloc) return SCL_ERR_INVALID_ARG;
    scl_allocator_t *alloc = params.alloc;
    scl_ml_gbdt_t *m = (scl_ml_gbdt_t *)scl_calloc(
        alloc, 1, sizeof(scl_ml_gbdt_t), alignof(max_align_t));
    if (scl_unlikely(!m)) return SCL_ERR_OUT_OF_MEMORY;
    m->params = params;
    m->alloc  = alloc;
    m->scratch = scl_alloc_arena_create(alloc, 8192, 0);
    if (!m->scratch) { scl_free(alloc, m); return SCL_ERR_OUT_OF_MEMORY; }
    *model = m;
    return SCL_OK;
}

void
scl_ml_gbdt_free(scl_ml_gbdt_t *model) {
    if (scl_unlikely(!model)) return;
    scl_allocator_t *a = model->alloc;
    for (size_t i = 0; i < model->n_estimators; i++)
        scl_free(a, model->trees[i].nodes);
    scl_free(a, model->trees);
    if (model->scratch) scl_alloc_arena_destroy(model->scratch);
    memset(model, 0, sizeof(*model));
    scl_free(a, model);
}

static int
scl_ml_gbdt_train_regression_tree(
    scl_ml_gbdt_internal_tree_t *tree_out,
    const scl_ml_dataset_t *ds,
    const SCL_ML_FLOAT *targets_override,
    size_t n_features_total,
    scl_ml_gbdt_params_t *params,
    scl_allocator_t *persist_alloc) {

    scl_allocator_t *tree_arena = scl_alloc_arena_create(persist_alloc, 8192, 0);
    if (!tree_arena) return -1;

    scl_ml_tree_params_t tree_params;
    memset(&tree_params, 0, sizeof(tree_params));
    tree_params.max_depth = params->max_depth;
    tree_params.min_samples_split = params->min_samples_split;
    tree_params.min_samples_leaf = params->min_samples_leaf;
    tree_params.criterion = SCL_ML_CRITERION_MSE;
    tree_params.max_features = n_features_total;
    tree_params.min_impurity_decrease = 0.0;
    tree_params.alloc = tree_arena;

    scl_ml_dataset_t tmp_ds;
    memset(&tmp_ds, 0, sizeof(tmp_ds));
    tmp_ds.data = ds->data;
    tmp_ds.targets = (SCL_ML_FLOAT *)targets_override;
    tmp_ds.n_rows = ds->n_rows;
    tmp_ds.n_cols = n_features_total;
    tmp_ds.row_stride = ds->row_stride;

    scl_ml_tree_t *t = NULL;
    scl_error_t err = scl_ml_tree_new(&t, tree_params);
    if (err != SCL_OK) { scl_alloc_arena_destroy(tree_arena); return -1; }

    err = scl_ml_tree_fit(t, &tmp_ds);
    if (err != SCL_OK) {
        scl_ml_tree_free(t);
        scl_alloc_arena_destroy(tree_arena);
        return -1;
    }

    tree_out->n_nodes = scl_ml_tree_get_n_nodes(t);
    tree_out->n_leaves = scl_ml_tree_get_n_leaves(t);
    tree_out->n_features = n_features_total;
    tree_out->is_classifier = 0;

    tree_out->nodes = (scl_ml_gbdt_node_t *)scl_alloc(
        persist_alloc, tree_out->n_nodes * sizeof(scl_ml_gbdt_node_t),
        alignof(max_align_t));
    if (!tree_out->nodes) {
        scl_ml_tree_free(t);
        scl_alloc_arena_destroy(tree_arena);
        return -1;
    }

    /* Zero-copy handoff: one memcpy, no serialize round-trip. */
    memcpy(tree_out->nodes, scl_ml_tree_get_nodes(t),
           tree_out->n_nodes * sizeof(scl_ml_tree_node_t));

    scl_ml_tree_free(t);
    scl_alloc_arena_destroy(tree_arena);
    return 0;
}

static SCL_ML_FLOAT
scl_ml_gbdt_tree_predict(const scl_ml_gbdt_internal_tree_t *tree,
                          const scl_ml_dataset_t *ds, size_t row) {
    int node_idx = 0;
    while (1) {
        scl_ml_gbdt_node_t *n = &tree->nodes[node_idx];
        if (n->feature_idx < 0)
            return n->value;
        if (ds->data[row * ds->row_stride + (size_t)n->feature_idx] <= n->threshold)
            node_idx = n->left_child;
        else
            node_idx = n->right_child;
    }
}

SCL_WARN_UNUSED scl_error_t
scl_ml_gbdt_fit(scl_ml_gbdt_t *model, const scl_ml_dataset_t *ds) {
    if (scl_unlikely(!model || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!ds->data || ds->n_rows == 0)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(scl_ml_dataset_has_missing(ds))) return SCL_ERR_ML_MISSING_DATA;

    scl_ml_simd_init();

    model->n_features = ds->n_cols;
    model->n_estimators = model->params.n_estimators;
    model->fitted = 0;
    scl_alloc_arena_reset(model->scratch);

    double sum_y = 0.0;
    for (size_t i = 0; i < ds->n_rows; i++)
        sum_y += (double)ds->targets[i];
    model->base_score = (SCL_ML_FLOAT)(sum_y / (double)ds->n_rows);

    scl_allocator_t *a = model->alloc;
    scl_allocator_t *s = model->scratch;

    SCL_ML_FLOAT *current_pred = (SCL_ML_FLOAT *)scl_calloc(
        s, ds->n_rows, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (!current_pred) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t i = 0; i < ds->n_rows; i++)
        current_pred[i] = model->base_score;

    SCL_ML_FLOAT *residuals = (SCL_ML_FLOAT *)scl_alloc(
        s, ds->n_rows * sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (!residuals) return SCL_ERR_OUT_OF_MEMORY;

    uint32_t seed = (uint32_t)model->params.random_seed;
    if (seed == (uint32_t)-1) seed = 42;

    model->trees = (scl_ml_gbdt_internal_tree_t *)scl_calloc(
        a, model->n_estimators, sizeof(scl_ml_gbdt_internal_tree_t), alignof(max_align_t));
    if (!model->trees) return SCL_ERR_OUT_OF_MEMORY;

    size_t subsample_n = (size_t)(model->params.subsample * (double)ds->n_rows);
    if (subsample_n < 1) subsample_n = 1;
    if (subsample_n > ds->n_rows) subsample_n = ds->n_rows;

    double lr = model->params.learning_rate;

    for (size_t round = 0; round < model->n_estimators; round++) {
        for (size_t i = 0; i < ds->n_rows; i++)
            residuals[i] = ds->targets[i] - current_pred[i];

        scl_ml_dataset_t subset_ds;
        memset(&subset_ds, 0, sizeof(subset_ds));
        SCL_ML_FLOAT *sub_residuals = residuals;

        if (model->params.subsample < 1.0 - 1e-6) {
            size_t *sub_idx = (size_t *)scl_alloc(
                s, subsample_n * sizeof(size_t), alignof(max_align_t));
            if (!sub_idx) {
                for (size_t j = 0; j < round; j++)
                    scl_free(a, model->trees[j].nodes);
                scl_free(a, model->trees); return SCL_ERR_OUT_OF_MEMORY;
            }

            uint32_t ls = seed;
            for (size_t i = 0; i < subsample_n; i++) {
                sub_idx[i] = (size_t)(scl_ml_gbdt_rand_uniform(&ls) *
                                      (double)ds->n_rows);
                if (sub_idx[i] >= ds->n_rows) sub_idx[i] = ds->n_rows - 1;
            }

            SCL_ML_FLOAT *sub_data = (SCL_ML_FLOAT *)scl_alloc(
                s, subsample_n * ds->n_cols * sizeof(SCL_ML_FLOAT), alignof(max_align_t));
            sub_residuals = (SCL_ML_FLOAT *)scl_alloc(
                s, subsample_n * sizeof(SCL_ML_FLOAT), alignof(max_align_t));
            if (!sub_data || !sub_residuals) {
                for (size_t j = 0; j < round; j++)
                    scl_free(a, model->trees[j].nodes);
                scl_free(a, model->trees); return SCL_ERR_OUT_OF_MEMORY;
            }

            for (size_t i = 0; i < subsample_n; i++) {
                size_t src = sub_idx[i];
                for (size_t j = 0; j < ds->n_cols; j++)
                    sub_data[i * ds->n_cols + j] =
                        ds->data[src * ds->row_stride + j];
                sub_residuals[i] = residuals[src];
            }

            subset_ds.data = sub_data;
            subset_ds.targets = sub_residuals;
            subset_ds.n_rows = subsample_n;
            subset_ds.n_cols = ds->n_cols;
            subset_ds.row_stride = ds->n_cols;

            int ret = scl_ml_gbdt_train_regression_tree(
                &model->trees[round], &subset_ds, sub_residuals,
                model->n_features, &model->params, a);

            if (ret != 0) {
                for (size_t j = 0; j <= round; j++)
                    scl_free(a, model->trees[j].nodes);
                scl_free(a, model->trees); return SCL_ERR_OUT_OF_MEMORY;
            }
        } else {
            subset_ds.data = ds->data;
            subset_ds.targets = residuals;
            subset_ds.n_rows = ds->n_rows;
            subset_ds.n_cols = ds->n_cols;
            subset_ds.row_stride = ds->row_stride;

            int ret = scl_ml_gbdt_train_regression_tree(
                &model->trees[round], &subset_ds, residuals,
                model->n_features, &model->params, a);

            if (ret != 0) {
                for (size_t j = 0; j <= round; j++)
                    scl_free(a, model->trees[j].nodes);
                scl_free(a, model->trees); return SCL_ERR_OUT_OF_MEMORY;
            }
        }

        for (size_t i = 0; i < ds->n_rows; i++) {
            SCL_ML_FLOAT tree_pred = scl_ml_gbdt_tree_predict(
                &model->trees[round], ds, i);
            current_pred[i] += (SCL_ML_FLOAT)(lr * (double)tree_pred);
        }

        seed = seed * 1103515245u + 12345u;
    }

    model->fitted = 1;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_gbdt_predict(scl_ml_gbdt_t *model, const scl_ml_dataset_t *ds,
                     SCL_ML_FLOAT *y_out) {
    if (scl_unlikely(!model || !ds || !y_out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(ds->n_cols != model->n_features)) return SCL_ERR_INVALID_ARG;

    double lr = model->params.learning_rate;

    for (size_t i = 0; i < ds->n_rows; i++) {
        double pred = (double)model->base_score;
        for (size_t t = 0; t < model->n_estimators; t++)
            pred += lr * (double)scl_ml_gbdt_tree_predict(
                &model->trees[t], ds, i);
        y_out[i] = (SCL_ML_FLOAT)pred;
    }

    return SCL_OK;
}

SCL_PURE size_t
scl_ml_gbdt_get_n_features(const scl_ml_gbdt_t *model) {
    return model ? model->n_features : 0;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_gbdt_save(const scl_ml_gbdt_t *model,
                  uint8_t **buf, size_t *len, scl_allocator_t *alloc) {
    if (scl_unlikely(!model || !buf || !len)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(!alloc)) return SCL_ERR_NULL_PTR;

    size_t trees_data_sz = 0;
    for (size_t i = 0; i < model->n_estimators; i++) {
        trees_data_sz += sizeof(size_t) * 2 + sizeof(int) +
                         model->trees[i].n_nodes * sizeof(scl_ml_gbdt_node_t);
    }

    scl_ml_serial_header_t hdr;
    hdr.magic     = SCL_ML_MAGIC;
    hdr.version   = SCL_ML_FORMAT_VERSION;
    hdr.algo_id   = SCL_ML_ALGO_GBDT;
    hdr.header_sz = sizeof(hdr);
    hdr.reserved  = 0;

    size_t payload_sz = sizeof(SCL_ML_FLOAT) + sizeof(double) +
                        sizeof(size_t) * 2 + trees_data_sz;
    size_t total = sizeof(hdr) + payload_sz + sizeof(uint32_t);

    uint8_t *buffer = (uint8_t *)scl_calloc(alloc, 1, total, alignof(max_align_t));
    if (!buffer) return SCL_ERR_OUT_OF_MEMORY;

    memcpy(buffer, &hdr, sizeof(hdr));
    size_t off = sizeof(hdr);

    memcpy(buffer + off, &model->base_score, sizeof(SCL_ML_FLOAT));
    off += sizeof(SCL_ML_FLOAT);
    double lr = model->params.learning_rate;
    memcpy(buffer + off, &lr, sizeof(double)); off += sizeof(double);
    memcpy(buffer + off, &model->n_estimators, sizeof(size_t));
    off += sizeof(size_t);
    memcpy(buffer + off, &model->n_features, sizeof(size_t));
    off += sizeof(size_t);

    for (size_t i = 0; i < model->n_estimators; i++) {
        memcpy(buffer + off, &model->trees[i].n_nodes, sizeof(size_t));
        off += sizeof(size_t);
        memcpy(buffer + off, &model->trees[i].n_leaves, sizeof(size_t));
        off += sizeof(size_t);
        memcpy(buffer + off, &model->trees[i].is_classifier, sizeof(int));
        off += sizeof(int);
        size_t nb = model->trees[i].n_nodes * sizeof(scl_ml_gbdt_node_t);
        memcpy(buffer + off, model->trees[i].nodes, nb);
        off += nb;
    }

    uint32_t crc = scl_ml_crc32c(buffer + sizeof(scl_ml_serial_header_t), payload_sz);
    memcpy(buffer + off, &crc, sizeof(crc));

    *buf = buffer;
    *len = total;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_gbdt_load(scl_ml_gbdt_t **model,
                  const uint8_t *buf, size_t len,
                  scl_ml_gbdt_params_t params) {
    if (scl_unlikely(!model || !buf)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(len < sizeof(scl_ml_serial_header_t) + sizeof(uint32_t)))
        return SCL_ERR_INVALID_ARG;

    const scl_ml_serial_header_t *hdr = (const scl_ml_serial_header_t *)buf;
    if (scl_unlikely(hdr->magic != SCL_ML_MAGIC ||
                     hdr->algo_id != SCL_ML_ALGO_GBDT))
        return SCL_ERR_INVALID_ARG;

    scl_ml_gbdt_t *m;
    scl_error_t err = scl_ml_gbdt_new(&m, params);
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

    memcpy(&m->base_score, buf + off, sizeof(SCL_ML_FLOAT));
    off += sizeof(SCL_ML_FLOAT);
    memcpy(&m->params.learning_rate, buf + off, sizeof(double));
    off += sizeof(double);
    memcpy(&m->n_estimators, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&m->n_features, buf + off, sizeof(size_t)); off += sizeof(size_t);

    scl_allocator_t *a = m->alloc;
    m->trees = (scl_ml_gbdt_internal_tree_t *)scl_calloc(
        a, m->n_estimators, sizeof(scl_ml_gbdt_internal_tree_t), alignof(max_align_t));
    if (!m->trees) { scl_ml_gbdt_free(m); return SCL_ERR_OUT_OF_MEMORY; }

    for (size_t i = 0; i < m->n_estimators; i++) {
        memcpy(&m->trees[i].n_nodes, buf + off, sizeof(size_t));
        off += sizeof(size_t);
        memcpy(&m->trees[i].n_leaves, buf + off, sizeof(size_t));
        off += sizeof(size_t);
        memcpy(&m->trees[i].is_classifier, buf + off, sizeof(int));
        off += sizeof(int);

        size_t nb = m->trees[i].n_nodes * sizeof(scl_ml_gbdt_node_t);
        m->trees[i].nodes = (scl_ml_gbdt_node_t *)scl_calloc(
            a, m->trees[i].n_nodes, sizeof(scl_ml_gbdt_node_t), alignof(max_align_t));
        if (!m->trees[i].nodes) {
            for (size_t j = 0; j < i; j++)
                scl_free(a, m->trees[j].nodes);
            scl_free(a, m->trees);
            memset(m, 0, sizeof(*m)); scl_free(a, m);
            return SCL_ERR_OUT_OF_MEMORY;
        }
        memcpy(m->trees[i].nodes, buf + off, nb);
        off += nb;
    }

    m->fitted = 1;
    *model = m;
    return SCL_OK;
}
