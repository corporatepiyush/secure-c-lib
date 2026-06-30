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

#include "scl_ml_kmeans.h"
#include "scl_ml_simd.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

typedef struct scl_ml_kmeans {
    scl_ml_kmeans_params_t params;
    SCL_ML_FLOAT *centroids;
    int    *labels;
    SCL_ML_FLOAT  inertia;
    size_t *counts;
    size_t  n_clusters;
    size_t  n_samples;
    size_t  n_features;
    int     fitted;
} scl_ml_kmeans_t;

static uint32_t
scl_ml_kmeans_prng(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static float
scl_ml_kmeans_randf(uint32_t *state)
{
    return (float)(scl_ml_kmeans_prng(state) & 0x7FFFFFFF) /
           (float)0x7FFFFFFF;
}

static scl_error_t
scl_ml_kmeans_init_pp(scl_ml_kmeans_t *model,
                       const SCL_ML_FLOAT *data, size_t row_stride,
                       uint32_t *rng)
{
    size_t n = model->n_samples;
    size_t d = model->n_features;
    size_t k = model->n_clusters;

    size_t first = (size_t)(scl_ml_kmeans_randf(rng) * (float)n);
    if (first >= n) first = n - 1;

    for (size_t dim = 0; dim < d; dim++)
        model->centroids[dim * k + 0] = data[first * row_stride + dim];

    float *min_dists = (float *)malloc(n * sizeof(float));
    if (!min_dists) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t ci = 0; ci < n; ci++)
        min_dists[ci] = scl_ml_simd.dist_l2_sq(
            (const float *)(data + ci * row_stride),
            (const float *)(model->centroids + 0),
            d);

    for (size_t c = 1; c < k; c++) {
        double total = 0.0;
        for (size_t ci = 0; ci < n; ci++)
            total += (double)(min_dists[ci] * min_dists[ci]);

        double target = (double)scl_ml_kmeans_randf(rng) * total;
        double cumulative = 0.0;
        size_t chosen = n - 1;
        for (size_t ci = 0; ci < n; ci++) {
            cumulative += (double)(min_dists[ci] * min_dists[ci]);
            if (cumulative >= target) {
                chosen = ci;
                break;
            }
        }

        for (size_t dim = 0; dim < d; dim++)
            model->centroids[dim * k + c] = data[chosen * row_stride + dim];

        for (size_t ci = 0; ci < n; ci++) {
            float dist = scl_ml_simd.dist_l2_sq(
                (const float *)(data + ci * row_stride),
                (const float *)(data + chosen * row_stride),
                d);
            if (dist < min_dists[ci]) min_dists[ci] = dist;
        }
    }

    free(min_dists);
    return SCL_OK;
}

static scl_error_t
scl_ml_kmeans_lloyd(scl_ml_kmeans_t *model,
                     const SCL_ML_FLOAT *data, size_t row_stride)
{
    size_t n = model->n_samples;
    size_t d = model->n_features;
    size_t k = model->n_clusters;
    float tol = (float)model->params.tol;

    float *dists = (float *)malloc(k * sizeof(float));
    SCL_ML_FLOAT *old_centroids = (SCL_ML_FLOAT *)malloc(d * k * sizeof(SCL_ML_FLOAT));
    if (!dists || !old_centroids) {
        free(dists); free(old_centroids);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    for (size_t iter = 0; iter < model->params.max_iter; iter++) {
        for (size_t ci = 0; ci < n; ci++) {
            for (size_t cj = 0; cj < k; cj++) {
                float dist = 0.0f;
                for (size_t dim = 0; dim < d; dim++) {
                    float diff = (float)(data[ci * row_stride + dim] -
                                model->centroids[dim * k + cj]);
                    dist += diff * diff;
                }
                dists[cj] = dist;
            }
            size_t best = 0;
            for (size_t cj = 1; cj < k; cj++) {
                if (dists[cj] < dists[best]) best = cj;
            }
            model->labels[ci] = (int)best;
        }

        memcpy(old_centroids, model->centroids,
               d * k * sizeof(SCL_ML_FLOAT));

        memset(model->centroids, 0, d * k * sizeof(SCL_ML_FLOAT));
        memset(model->counts, 0, k * sizeof(size_t));

        for (size_t ci = 0; ci < n; ci++) {
            int cj = model->labels[ci];
            if (scl_unlikely(cj < 0 || (size_t)cj >= k)) continue;
            model->counts[cj]++;
            for (size_t dim = 0; dim < d; dim++)
                model->centroids[dim * k + cj] +=
                    data[ci * row_stride + dim];
        }

        for (size_t cj = 0; cj < k; cj++) {
            if (model->counts[cj] > 0) {
                float inv = 1.0f / (float)model->counts[cj];
                for (size_t dim = 0; dim < d; dim++)
                    model->centroids[dim * k + cj] *= inv;
            }
        }

        double shift = 0.0;
        for (size_t cj = 0; cj < k; cj++) {
            for (size_t dim = 0; dim < d; dim++) {
                float diff = (float)(model->centroids[dim * k + cj] -
                            old_centroids[dim * k + cj]);
                shift += (double)(diff * diff);
            }
        }

        if (shift < (double)tol) break;
    }

    model->inertia = 0.0f;
    for (size_t ci = 0; ci < n; ci++) {
        int cj = model->labels[ci];
        if (scl_unlikely(cj < 0 || (size_t)cj >= k)) continue;
        for (size_t dim = 0; dim < d; dim++) {
            float diff = (float)(data[ci * row_stride + dim] -
                        model->centroids[dim * k + cj]);
            model->inertia += diff * diff;
        }
    }

    free(dists);
    free(old_centroids);
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_kmeans_new(scl_ml_kmeans_t **model, scl_ml_kmeans_params_t params)
{
    if (scl_unlikely(!model)) return SCL_ERR_NULL_PTR;

    scl_ml_kmeans_t *m = (scl_ml_kmeans_t *)calloc(1, sizeof(scl_ml_kmeans_t));
    if (scl_unlikely(!m)) return SCL_ERR_OUT_OF_MEMORY;

    if (params.n_clusters == 0) params.n_clusters = 8;
    if (params.max_iter == 0)   params.max_iter = 300;
    if (params.n_init == 0)     params.n_init = 10;

    m->params = params;
    *model = m;
    return SCL_OK;
}

void
scl_ml_kmeans_free(scl_ml_kmeans_t *model)
{
    if (scl_unlikely(!model)) return;
    free(model->centroids);
    free(model->labels);
    free(model->counts);
    memset(model, 0, sizeof(*model));
    free(model);
}

SCL_WARN_UNUSED scl_error_t
scl_ml_kmeans_fit(scl_ml_kmeans_t *model, const scl_ml_dataset_t *ds)
{
    if (scl_unlikely(!model || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!ds->data || ds->n_rows == 0)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(scl_ml_dataset_has_missing(ds))) return SCL_ERR_ML_MISSING_DATA;

    scl_ml_simd_init();

    size_t n = ds->n_rows;
    size_t d = ds->n_cols;
    size_t k = model->params.n_clusters;

    if (k > n) k = n;
    if (k == 0) return SCL_ERR_INVALID_ARG;

    free(model->centroids);
    free(model->labels);
    free(model->counts);

    model->n_samples = n;
    model->n_features = d;
    model->n_clusters = k;

    model->centroids = (SCL_ML_FLOAT *)calloc(d * k, sizeof(SCL_ML_FLOAT));
    model->labels    = (int *)calloc(n, sizeof(int));
    model->counts    = (size_t *)calloc(k, sizeof(size_t));
    if (!model->centroids || !model->labels || !model->counts) {
        free(model->centroids); free(model->labels); free(model->counts);
        model->centroids = NULL; model->labels = NULL; model->counts = NULL;
        return SCL_ERR_OUT_OF_MEMORY;
    }

    uint32_t rng;
    if (model->params.random_seed < 0) {
        rng = (uint32_t)(uintptr_t)ds + (uint32_t)n;
        rng = (rng ^ (rng >> 16)) * 0x45d9f3b;
    } else {
        rng = (uint32_t)model->params.random_seed;
    }

    SCL_ML_FLOAT *best_centroids = NULL;
    int   *best_labels = NULL;
    SCL_ML_FLOAT best_inertia = FLT_MAX;

    for (int init = 0; init < model->params.n_init; init++) {
        scl_error_t err = scl_ml_kmeans_init_pp(model, ds->data,
                                                  ds->row_stride, &rng);
        if (err != SCL_OK) return err;

        err = scl_ml_kmeans_lloyd(model, ds->data, ds->row_stride);
        if (err != SCL_OK) return err;

        if (model->inertia < best_inertia || init == 0) {
            best_inertia = model->inertia;
            if (!best_centroids) {
                best_centroids = (SCL_ML_FLOAT *)malloc(
                    d * k * sizeof(SCL_ML_FLOAT));
                best_labels = (int *)malloc(n * sizeof(int));
                if (!best_centroids || !best_labels) {
                    free(best_centroids); free(best_labels);
                    return SCL_ERR_OUT_OF_MEMORY;
                }
            }
            memcpy(best_centroids, model->centroids,
                   d * k * sizeof(SCL_ML_FLOAT));
            memcpy(best_labels, model->labels,
                   n * sizeof(int));
        }
    }

    if (best_centroids) {
        memcpy(model->centroids, best_centroids,
               d * k * sizeof(SCL_ML_FLOAT));
        memcpy(model->labels, best_labels,
               n * sizeof(int));
        model->inertia = best_inertia;
        free(best_centroids);
        free(best_labels);
    }

    model->fitted = 1;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_kmeans_predict(scl_ml_kmeans_t *model, const scl_ml_dataset_t *ds,
                       int *y_out)
{
    if (scl_unlikely(!model || !ds || !y_out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(ds->n_cols != model->n_features)) return SCL_ERR_INVALID_ARG;

    size_t n = ds->n_rows;
    size_t d = model->n_features;
    size_t k = model->n_clusters;

    float *dists = (float *)malloc(k * sizeof(float));
    if (!dists) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t ci = 0; ci < n; ci++) {
        for (size_t cj = 0; cj < k; cj++) {
            float dist = 0.0f;
            for (size_t dim = 0; dim < d; dim++) {
                float diff = (float)(ds->data[ci * ds->row_stride + dim] -
                            model->centroids[dim * k + cj]);
                dist += diff * diff;
            }
            dists[cj] = dist;
        }
        size_t best = 0;
        for (size_t cj = 1; cj < k; cj++) {
            if (dists[cj] < dists[best]) best = cj;
        }
        y_out[ci] = (int)best;
    }

    free(dists);
    return SCL_OK;
}

SCL_PURE size_t
scl_ml_kmeans_get_n_clusters(const scl_ml_kmeans_t *model)
{
    return model ? model->n_clusters : 0;
}

SCL_PURE const int *
scl_ml_kmeans_get_labels(const scl_ml_kmeans_t *model)
{
    return model ? model->labels : NULL;
}

SCL_PURE SCL_ML_FLOAT
scl_ml_kmeans_get_inertia(const scl_ml_kmeans_t *model)
{
    return model ? model->inertia : 0.0f;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_kmeans_save(const scl_ml_kmeans_t *model,
                    uint8_t **buf, size_t *len, scl_allocator_t *alloc)
{
    (void)alloc;
    if (scl_unlikely(!model || !buf || !len)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;

    size_t centroids_bytes = model->n_features * model->n_clusters *
                             sizeof(SCL_ML_FLOAT);
    size_t labels_bytes = model->n_samples * sizeof(int);
    size_t counts_bytes = model->n_clusters * sizeof(size_t);

    scl_ml_serial_header_t hdr;
    hdr.magic     = SCL_ML_MAGIC;
    hdr.version   = SCL_ML_FORMAT_VERSION;
    hdr.algo_id   = SCL_ML_ALGO_KMEANS;
    hdr.header_sz = sizeof(hdr);
    hdr.reserved  = 0;

    size_t payload_sz = sizeof(size_t) * 3 + sizeof(SCL_ML_FLOAT) +
                        centroids_bytes + labels_bytes + counts_bytes;
    size_t total = sizeof(hdr) + payload_sz + sizeof(uint32_t);

    uint8_t *buffer = (uint8_t *)calloc(1, total);
    if (!buffer) return SCL_ERR_OUT_OF_MEMORY;

    memcpy(buffer, &hdr, sizeof(hdr));
    size_t off = sizeof(hdr);

    memcpy(buffer + off, &model->n_clusters, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->n_samples, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->n_features, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->inertia, sizeof(SCL_ML_FLOAT)); off += sizeof(SCL_ML_FLOAT);
    memcpy(buffer + off, model->centroids, centroids_bytes); off += centroids_bytes;
    memcpy(buffer + off, model->labels, labels_bytes); off += labels_bytes;
    memcpy(buffer + off, model->counts, counts_bytes); off += counts_bytes;

    uint32_t crc = 0;
    memcpy(buffer + off, &crc, sizeof(crc));

    *buf = buffer;
    *len = total;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_kmeans_load(scl_ml_kmeans_t **model,
                    const uint8_t *buf, size_t len,
                    scl_ml_kmeans_params_t params)
{
    if (scl_unlikely(!model || !buf)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(len < sizeof(scl_ml_serial_header_t) + sizeof(uint32_t)))
        return SCL_ERR_INVALID_ARG;

    const scl_ml_serial_header_t *hdr = (const scl_ml_serial_header_t *)buf;
    if (scl_unlikely(hdr->magic != SCL_ML_MAGIC ||
                     hdr->algo_id != SCL_ML_ALGO_KMEANS))
        return SCL_ERR_INVALID_ARG;

    size_t off = sizeof(*hdr);
    size_t n_clusters = 0, n_samples = 0, n_features = 0;
    memcpy(&n_clusters, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&n_samples, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&n_features, buf + off, sizeof(size_t)); off += sizeof(size_t);

    if (params.n_clusters == 0) params.n_clusters = n_clusters;
    if (params.max_iter == 0)   params.max_iter = 300;
    if (params.n_init == 0)     params.n_init = 10;

    scl_ml_kmeans_t *m = (scl_ml_kmeans_t *)calloc(1, sizeof(scl_ml_kmeans_t));
    if (!m) return SCL_ERR_OUT_OF_MEMORY;
    m->params = params;
    m->n_clusters = n_clusters;
    m->n_samples = n_samples;
    m->n_features = n_features;

    size_t centroids_bytes = n_features * n_clusters * sizeof(SCL_ML_FLOAT);
    size_t labels_bytes = n_samples * sizeof(int);
    size_t counts_bytes = n_clusters * sizeof(size_t);

    m->centroids = (SCL_ML_FLOAT *)calloc(1, centroids_bytes);
    m->labels = (int *)calloc(1, labels_bytes);
    m->counts = (size_t *)calloc(1, counts_bytes);
    if (!m->centroids || !m->labels || !m->counts) {
        free(m->centroids); free(m->labels); free(m->counts); free(m);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    memcpy(&m->inertia, buf + off, sizeof(SCL_ML_FLOAT)); off += sizeof(SCL_ML_FLOAT);
    memcpy(m->centroids, buf + off, centroids_bytes); off += centroids_bytes;
    memcpy(m->labels, buf + off, labels_bytes); off += labels_bytes;
    memcpy(m->counts, buf + off, counts_bytes);

    m->fitted = 1;
    *model = m;
    return SCL_OK;
}
