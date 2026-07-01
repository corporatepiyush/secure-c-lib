/*
 * Copyright 2026 Piyush Katariya
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "scl_ml_knn.h"
#include "scl_ml_simd.h"
#include "scl_alloc_arena.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

typedef struct scl_ml_knn {
    scl_ml_knn_params_t params;
    SCL_ML_FLOAT *train_data;
    SCL_ML_FLOAT *train_targets;
    size_t        n_samples;
    size_t        n_features;
    int           fitted;
    scl_allocator_t *alloc;
    scl_allocator_t *scratch;
} scl_ml_knn_t;

static SCL_COLD_PATH scl_error_t
scl_ml_knn_topk_min(const float *vals, uint32_t *indices_out,
                     float *dists_out, size_t n, size_t k)
{
    if (k == 0 || n == 0) return SCL_OK;
    if (k > n) k = n;

    for (size_t i = 0; i < k; i++) {
        indices_out[i] = (uint32_t)i;
        dists_out[i] = vals[i];
    }

    for (size_t i = k; i < n; i++) {
        float v = vals[i];
        size_t max_idx = 0;
        for (size_t j = 1; j < k; j++) {
            if (dists_out[j] > dists_out[max_idx])
                max_idx = j;
        }
        if (v < dists_out[max_idx]) {
            dists_out[max_idx] = v;
            indices_out[max_idx] = (uint32_t)i;
        }
    }

    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_knn_new(scl_ml_knn_t **model, scl_ml_knn_params_t params)
{
    if (scl_unlikely(!model)) return SCL_ERR_NULL_PTR;
    if (!params.alloc) return SCL_ERR_INVALID_ARG;
    scl_allocator_t *alloc = params.alloc;
    scl_ml_knn_t *m = (scl_ml_knn_t *)scl_calloc(
        alloc, 1, sizeof(scl_ml_knn_t), alignof(max_align_t));
    if (scl_unlikely(!m)) return SCL_ERR_OUT_OF_MEMORY;

    if (params.k == 0) params.k = 5;
    m->params = params;
    m->alloc  = alloc;
    m->scratch = scl_alloc_arena_create(alloc, 8192, 0);
    if (!m->scratch) { scl_free(alloc, m); return SCL_ERR_OUT_OF_MEMORY; }
    *model = m;
    return SCL_OK;
}

void
scl_ml_knn_free(scl_ml_knn_t *model)
{
    if (scl_unlikely(!model)) return;
    scl_allocator_t *a = model->alloc;
    scl_free(a, model->train_data);
    scl_free(a, model->train_targets);
    if (model->scratch) scl_alloc_arena_destroy(model->scratch);
    memset(model, 0, sizeof(*model));
    scl_free(a, model);
}

SCL_WARN_UNUSED scl_error_t
scl_ml_knn_fit(scl_ml_knn_t *model, const scl_ml_dataset_t *ds)
{
    if (scl_unlikely(!model || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!ds->data || ds->n_rows == 0)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(scl_ml_dataset_has_missing(ds))) return SCL_ERR_ML_MISSING_DATA;

    scl_ml_simd_init();

    size_t n = ds->n_rows;
    size_t d = ds->n_cols;

    scl_alloc_arena_reset(model->scratch);
    scl_allocator_t *a = model->alloc;

    scl_free(a, model->train_data);
    scl_free(a, model->train_targets);

    model->train_data = (SCL_ML_FLOAT *)scl_calloc(a, n * d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (scl_unlikely(!model->train_data)) return SCL_ERR_OUT_OF_MEMORY;

    model->train_targets = (SCL_ML_FLOAT *)scl_calloc(a, n, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (scl_unlikely(!model->train_targets)) {
        scl_free(a, model->train_data);
        model->train_data = NULL;
        return SCL_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < n; i++) {
        size_t base = i * ds->row_stride;
        for (size_t j = 0; j < d; j++)
            model->train_data[i * d + j] = ds->data[base + j];
        model->train_targets[i] = ds->targets[i];
    }

    model->n_samples = n;
    model->n_features = d;
    model->fitted = 1;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_knn_predict(scl_ml_knn_t *model, const scl_ml_dataset_t *ds,
                    SCL_ML_FLOAT *y_out)
{
    if (scl_unlikely(!model || !ds || !y_out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(ds->n_cols != model->n_features)) return SCL_ERR_INVALID_ARG;

    size_t nq = ds->n_rows;
    size_t nt = model->n_samples;
    size_t d  = model->n_features;
    size_t k  = model->params.k;
    if (k > nt) k = nt;
    if (k == 0) { memset(y_out, 0, nq * sizeof(SCL_ML_FLOAT)); return SCL_OK; }

    scl_alloc_arena_reset(model->scratch);
    float *dist_row = (float *)scl_alloc(model->scratch, nt * sizeof(float), alignof(max_align_t));
    uint32_t *indices = (uint32_t *)scl_alloc(model->scratch, k * sizeof(uint32_t), alignof(max_align_t));
    float *dists_k = (float *)scl_alloc(model->scratch, k * sizeof(float), alignof(max_align_t));
    float seen_labels[32];
    float seen_weights[32];
    int   seen_counts[32];
    if (!dist_row || !indices || !dists_k) {
        return SCL_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < nq; i++) {
        size_t q_base = i * ds->row_stride;

        for (size_t j = 0; j < nt; j++) {
            dist_row[j] = scl_ml_simd.dist_l2_sq(
                (const float *)(ds->data + q_base),
                (const float *)(model->train_data + j * d),
                d);
        }

        scl_ml_knn_topk_min(dist_row, indices, dists_k, nt, k);

        int n_seen = 0;
        if (model->params.weights == 0) {
            for (size_t j = 0; j < 32; j++) seen_counts[j] = 0;

            for (size_t j = 0; j < k; j++) {
                float lbl = model->train_targets[indices[j]];
                int found = 0;
                for (int s = 0; s < n_seen; s++) {
                    if (seen_labels[s] == lbl) {
                        seen_counts[s]++;
                        found = 1;
                        break;
                    }
                }
                if (!found && n_seen < 32) {
                    seen_labels[n_seen] = lbl;
                    seen_counts[n_seen] = 1;
                    n_seen++;
                }
            }

            int best_s = 0;
            for (int s = 1; s < n_seen; s++) {
                if (seen_counts[s] > seen_counts[best_s])
                    best_s = s;
            }
            y_out[i] = seen_labels[best_s];
        } else {
            for (size_t j = 0; j < 32; j++) seen_weights[j] = 0.0f;

            for (size_t j = 0; j < k; j++) {
                float w = 1.0f / (dists_k[j] + SCL_ML_EPSILON);
                float lbl = model->train_targets[indices[j]];
                int found = 0;
                for (int s = 0; s < n_seen; s++) {
                    if (seen_labels[s] == lbl) {
                        seen_weights[s] += w;
                        found = 1;
                        break;
                    }
                }
                if (!found && n_seen < 32) {
                    seen_labels[n_seen] = lbl;
                    seen_weights[n_seen] = w;
                    n_seen++;
                }
            }

            int best_s = 0;
            for (int s = 1; s < n_seen; s++) {
                if (seen_weights[s] > seen_weights[best_s])
                    best_s = s;
            }
            y_out[i] = seen_labels[best_s];
        }
    }

    return SCL_OK;
}

SCL_PURE size_t
scl_ml_knn_get_n_features(const scl_ml_knn_t *model)
{
    return model ? model->n_features : 0;
}

SCL_PURE size_t
scl_ml_knn_get_n_samples(const scl_ml_knn_t *model)
{
    return model ? model->n_samples : 0;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_knn_save(const scl_ml_knn_t *model,
                 uint8_t **buf, size_t *len, scl_allocator_t *alloc)
{
    if (scl_unlikely(!model || !buf || !len)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(!alloc)) return SCL_ERR_NULL_PTR;

    size_t data_bytes = model->n_samples * model->n_features * sizeof(SCL_ML_FLOAT);
    size_t tgt_bytes  = model->n_samples * sizeof(SCL_ML_FLOAT);

    scl_ml_serial_header_t hdr;
    hdr.magic     = SCL_ML_MAGIC;
    hdr.version   = SCL_ML_FORMAT_VERSION;
    hdr.algo_id   = SCL_ML_ALGO_KNN;
    hdr.header_sz = sizeof(hdr);
    hdr.reserved  = 0;

    size_t payload_sz = sizeof(size_t) * 2 + data_bytes + tgt_bytes;
    size_t total = sizeof(hdr) + payload_sz + sizeof(uint32_t);

    uint8_t *buffer = (uint8_t *)scl_calloc(alloc, 1, total, alignof(max_align_t));
    if (!buffer) return SCL_ERR_OUT_OF_MEMORY;

    memcpy(buffer, &hdr, sizeof(hdr));
    size_t off = sizeof(hdr);

    memcpy(buffer + off, &model->n_samples, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->n_features, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, model->train_data, data_bytes); off += data_bytes;
    memcpy(buffer + off, model->train_targets, tgt_bytes); off += tgt_bytes;

    uint32_t crc = scl_ml_crc32c(buffer + sizeof(scl_ml_serial_header_t), payload_sz);
    memcpy(buffer + off, &crc, sizeof(crc));

    *buf = buffer;
    *len = total;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_knn_load(scl_ml_knn_t **model,
                 const uint8_t *buf, size_t len,
                 scl_ml_knn_params_t params)
{
    if (scl_unlikely(!model || !buf)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(len < sizeof(scl_ml_serial_header_t) + sizeof(uint32_t)))
        return SCL_ERR_INVALID_ARG;

    const scl_ml_serial_header_t *hdr = (const scl_ml_serial_header_t *)buf;
    if (scl_unlikely(hdr->magic != SCL_ML_MAGIC || hdr->algo_id != SCL_ML_ALGO_KNN))
        return SCL_ERR_INVALID_ARG;

    /* Verify payload integrity before any allocation/parsing. */
    uint32_t stored_crc = 0;
    memcpy(&stored_crc, buf + len - sizeof(uint32_t), sizeof(uint32_t));
    uint32_t expected_crc = scl_ml_crc32c(
        buf + sizeof(scl_ml_serial_header_t),
        len - sizeof(scl_ml_serial_header_t) - sizeof(uint32_t));
    if (scl_unlikely(stored_crc != expected_crc))
        return SCL_ERR_INVALID_ARG;

    size_t off = sizeof(*hdr);
    size_t n_samples = 0, n_features = 0;
    memcpy(&n_samples, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&n_features, buf + off, sizeof(size_t)); off += sizeof(size_t);

    if (params.k == 0) params.k = 5;

    scl_ml_knn_t *m;
    scl_error_t nerr = scl_ml_knn_new(&m, params);
    if (nerr != SCL_OK) return nerr;
    scl_allocator_t *a = m->alloc;
    m->n_samples = n_samples;
    m->n_features = n_features;

    size_t data_bytes = n_samples * n_features * sizeof(SCL_ML_FLOAT);
    size_t tgt_bytes  = n_samples * sizeof(SCL_ML_FLOAT);

    m->train_data = (SCL_ML_FLOAT *)scl_calloc(a, 1, data_bytes, alignof(max_align_t));
    m->train_targets = (SCL_ML_FLOAT *)scl_calloc(a, 1, tgt_bytes, alignof(max_align_t));
    if (!m->train_data || !m->train_targets) {
        scl_free(a, m->train_data); scl_free(a, m->train_targets);
        memset(m, 0, sizeof(*m)); scl_free(a, m);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    memcpy(m->train_data, buf + off, data_bytes); off += data_bytes;
    memcpy(m->train_targets, buf + off, tgt_bytes);

    m->fitted = 1;
    *model = m;
    return SCL_OK;
}
