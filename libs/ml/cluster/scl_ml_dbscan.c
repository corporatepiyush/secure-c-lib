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

#include "scl_ml_dbscan.h"
#include "scl_ml_simd.h"
#include "scl_alloc_arena.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

#define SCL_ML_DBSCAN_UNASSIGNED (-2)
#define SCL_ML_DBSCAN_NOISE     (-1)

typedef struct scl_ml_dbscan {
    scl_ml_dbscan_params_t params;
    int    *labels;
    size_t  n_clusters;
    size_t  n_samples;
    size_t  n_features;
    int     fitted;
    scl_allocator_t *alloc;
    scl_allocator_t *scratch;
} scl_ml_dbscan_t;

static size_t
scl_ml_dbscan_region_query(const SCL_ML_FLOAT *data, size_t n, size_t d,
                            size_t row_stride, size_t p, float eps_sq,
                            size_t *neighbors)
{
    size_t count = 0;
    for (size_t j = 0; j < n; j++) {
        if (j == p) continue;
        float dist = scl_ml_simd.dist_l2_sq(
            (const float *)(data + p * row_stride),
            (const float *)(data + j * row_stride),
            d);
        if (dist <= eps_sq)
            neighbors[count++] = j;
    }
    return count;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_dbscan_new(scl_ml_dbscan_t **model, scl_ml_dbscan_params_t params)
{
    if (scl_unlikely(!model)) return SCL_ERR_NULL_PTR;
    if (!params.alloc) return SCL_ERR_INVALID_ARG;
    scl_allocator_t *alloc = params.alloc;
    scl_ml_dbscan_t *m = (scl_ml_dbscan_t *)scl_calloc(
        alloc, 1, sizeof(scl_ml_dbscan_t), alignof(max_align_t));
    if (scl_unlikely(!m)) return SCL_ERR_OUT_OF_MEMORY;

    m->scratch = scl_alloc_arena_create(alloc, 8192, 0);
    if (!m->scratch) { scl_free(alloc, m); return SCL_ERR_OUT_OF_MEMORY; }

    if (params.eps <= 0.0) params.eps = 0.5;
    if (params.min_pts == 0) params.min_pts = 5;

    m->params = params;
    m->alloc  = alloc;
    *model = m;
    return SCL_OK;
}

void
scl_ml_dbscan_free(scl_ml_dbscan_t *model)
{
    if (scl_unlikely(!model)) return;
    scl_allocator_t *a = model->alloc;
    scl_free(a, model->labels);
    if (model->scratch) scl_alloc_arena_destroy(model->scratch);
    memset(model, 0, sizeof(*model));
    scl_free(a, model);
}

SCL_WARN_UNUSED scl_error_t
scl_ml_dbscan_fit(scl_ml_dbscan_t *model, const scl_ml_dataset_t *ds)
{
    if (scl_unlikely(!model || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!ds->data || ds->n_rows == 0)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(scl_ml_dataset_has_missing(ds))) return SCL_ERR_ML_MISSING_DATA;

    scl_ml_simd_init();

    size_t n = ds->n_rows;
    size_t d = ds->n_cols;
    size_t min_pts = model->params.min_pts;
    float eps_sq = (float)(model->params.eps * model->params.eps);

    scl_alloc_arena_reset(model->scratch);

    scl_allocator_t *a = model->alloc;

    scl_free(a, model->labels);
    model->n_samples = n;
    model->n_features = d;
    model->n_clusters = 0;

    model->labels = (int *)scl_alloc(a, n * sizeof(int), alignof(max_align_t));
    if (!model->labels) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t i = 0; i < n; i++)
        model->labels[i] = SCL_ML_DBSCAN_UNASSIGNED;

    size_t *neighbors = (size_t *)scl_alloc(model->scratch, n * sizeof(size_t), alignof(max_align_t));
    size_t *queue = (size_t *)scl_alloc(model->scratch, n * sizeof(size_t), alignof(max_align_t));
    if (!neighbors || !queue) {
        return SCL_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < n; i++) {
        if (model->labels[i] != SCL_ML_DBSCAN_UNASSIGNED)
            continue;

        size_t n_neighbors = scl_ml_dbscan_region_query(
            ds->data, n, d, ds->row_stride, i, eps_sq, neighbors);

        if (n_neighbors < min_pts) {
            model->labels[i] = SCL_ML_DBSCAN_NOISE;
            continue;
        }

        int cluster_id = (int)(++model->n_clusters);
        model->labels[i] = cluster_id;

        size_t qhead = 0, qtail = 0;
        for (size_t j = 0; j < n_neighbors; j++) {
            if (neighbors[j] != i)
                queue[qtail++] = neighbors[j];
        }

        while (qhead < qtail) {
            size_t q = queue[qhead++];

            if (model->labels[q] == SCL_ML_DBSCAN_NOISE) {
                model->labels[q] = cluster_id;
                continue;
            }
            if (model->labels[q] != SCL_ML_DBSCAN_UNASSIGNED)
                continue;

            model->labels[q] = cluster_id;

            size_t nn = scl_ml_dbscan_region_query(
                ds->data, n, d, ds->row_stride, q, eps_sq, neighbors);

            if (nn >= min_pts) {
                for (size_t j = 0; j < nn; j++) {
                    size_t nb = neighbors[j];
                    if (model->labels[nb] == SCL_ML_DBSCAN_UNASSIGNED ||
                        model->labels[nb] == SCL_ML_DBSCAN_NOISE) {
                        if (qtail < n)
                            queue[qtail++] = nb;
                    }
                }
            }
        }
    }

    model->fitted = 1;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_dbscan_predict(scl_ml_dbscan_t *model, const scl_ml_dataset_t *ds,
                       int *y_out)
{
    if (scl_unlikely(!model || !ds || !y_out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(ds->n_cols != model->n_features)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(ds->n_rows != model->n_samples)) return SCL_ERR_INVALID_ARG;

    scl_alloc_arena_reset(model->scratch);

    memcpy(y_out, model->labels, model->n_samples * sizeof(int));
    return SCL_OK;
}

SCL_PURE size_t
scl_ml_dbscan_get_n_clusters(const scl_ml_dbscan_t *model)
{
    return model ? model->n_clusters : 0;
}

SCL_PURE const int *
scl_ml_dbscan_get_labels(const scl_ml_dbscan_t *model)
{
    return model ? model->labels : NULL;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_dbscan_save(const scl_ml_dbscan_t *model,
                    uint8_t **buf, size_t *len, scl_allocator_t *alloc)
{
    if (scl_unlikely(!model || !buf || !len)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(!alloc)) return SCL_ERR_NULL_PTR;

    size_t labels_bytes = model->n_samples * sizeof(int);

    scl_ml_serial_header_t hdr;
    hdr.magic     = SCL_ML_MAGIC;
    hdr.version   = SCL_ML_FORMAT_VERSION;
    hdr.algo_id   = SCL_ML_ALGO_DBSCAN;
    hdr.header_sz = sizeof(hdr);
    hdr.reserved  = 0;

    size_t payload_sz = sizeof(size_t) * 3 + labels_bytes;
    size_t total = sizeof(hdr) + payload_sz + sizeof(uint32_t);

    uint8_t *buffer = (uint8_t *)scl_calloc(alloc, 1, total, alignof(max_align_t));
    if (!buffer) return SCL_ERR_OUT_OF_MEMORY;

    memcpy(buffer, &hdr, sizeof(hdr));
    size_t off = sizeof(hdr);

    memcpy(buffer + off, &model->n_clusters, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->n_samples, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->n_features, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, model->labels, labels_bytes); off += labels_bytes;

    uint32_t crc = scl_ml_crc32c(buffer + sizeof(scl_ml_serial_header_t), payload_sz);
    memcpy(buffer + off, &crc, sizeof(crc));

    *buf = buffer;
    *len = total;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_dbscan_load(scl_ml_dbscan_t **model,
                    const uint8_t *buf, size_t len,
                    scl_ml_dbscan_params_t params)
{
    if (scl_unlikely(!model || !buf)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(len < sizeof(scl_ml_serial_header_t) + sizeof(uint32_t)))
        return SCL_ERR_INVALID_ARG;

    const scl_ml_serial_header_t *hdr = (const scl_ml_serial_header_t *)buf;
    if (scl_unlikely(hdr->magic != SCL_ML_MAGIC ||
                     hdr->algo_id != SCL_ML_ALGO_DBSCAN))
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
    size_t n_clusters = 0, n_samples = 0, n_features = 0;
    memcpy(&n_clusters, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&n_samples, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&n_features, buf + off, sizeof(size_t)); off += sizeof(size_t);

    if (params.eps <= 0.0)     params.eps = 0.5;
    if (params.min_pts == 0)   params.min_pts = 5;

    scl_ml_dbscan_t *m;
    scl_error_t nerr = scl_ml_dbscan_new(&m, params);
    if (nerr != SCL_OK) return nerr;
    scl_allocator_t *a = m->alloc;
    m->n_clusters = n_clusters;
    m->n_samples = n_samples;
    m->n_features = n_features;

    size_t labels_bytes = n_samples * sizeof(int);
    m->labels = (int *)scl_calloc(a, 1, labels_bytes, alignof(max_align_t));
    if (!m->labels) { memset(m, 0, sizeof(*m)); scl_free(a, m); return SCL_ERR_OUT_OF_MEMORY; }

    memcpy(m->labels, buf + off, labels_bytes);

    m->fitted = 1;
    *model = m;
    return SCL_OK;
}
