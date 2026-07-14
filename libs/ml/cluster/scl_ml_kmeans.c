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
#include "scl_alloc_arena.h"
#include "scl_math.h"
#include "scl_ml_simd.h"
#include "scl_stdlib.h"
#include "scl_string.h"
#include <float.h>

typedef struct scl_ml_kmeans {
  scl_ml_kmeans_params_t params;
  SCL_ML_FLOAT
  *centroids; /* [k * d], row-major: centroid c at [c*d] (contiguous) */
  int *labels;
  SCL_ML_FLOAT inertia;
  size_t *counts;
  size_t n_clusters;
  size_t n_samples;
  size_t n_features;
  int fitted;
  /* Fit-time scratch: allocated once in _fit, reused across all n_init
   * restarts and iterations to keep the hot path allocation-free. */
  SCL_ML_FLOAT *old_centroids; /* [k * d] */
  float *dists;                /* [k] candidate distances */
  scl_allocator_t *alloc;      /* pluggable allocator (arena/slab/tlsf/pool) */
  scl_allocator_t *scratch;    /* arena for temporary fit/predict allocations */
} scl_ml_kmeans_t;

static uint32_t scl_ml_kmeans_prng(uint32_t *state) {
  uint32_t x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

static float scl_ml_kmeans_randf(uint32_t *state) {
  return (float)(scl_ml_kmeans_prng(state) & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

static scl_error_t scl_ml_kmeans_init_pp(scl_ml_kmeans_t *model,
                                         const SCL_ML_FLOAT *data,
                                         size_t row_stride, uint32_t *rng) {
  size_t n = model->n_samples;
  size_t d = model->n_features;
  size_t k = model->n_clusters;

  size_t first = (size_t)(scl_ml_kmeans_randf(rng) * (float)n);
  if (first >= n)
    first = n - 1;

  /* Row-major: centroid 0 occupies centroids[0..d-1] contiguously */
  for (size_t dim = 0; dim < d; dim++)
    model->centroids[0 * d + dim] = data[first * row_stride + dim];

  float *min_dists =
      (float *)scl_alloc(model->scratch, n * sizeof(float), SCL_ML_ALIGNMENT);
  if (!min_dists)
    return SCL_ERR_OUT_OF_MEMORY;

  for (size_t ci = 0; ci < n; ci++)
    min_dists[ci] =
        scl_ml_simd.dist_l2_sq((const float *)(data + ci * row_stride),
                               (const float *)(model->centroids + 0 * d), d);

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
      model->centroids[c * d + dim] = data[chosen * row_stride + dim];

    for (size_t ci = 0; ci < n; ci++) {
      float dist = scl_ml_simd.dist_l2_sq(
          (const float *)(data + ci * row_stride),
          (const float *)(data + chosen * row_stride), d);
      if (dist < min_dists[ci])
        min_dists[ci] = dist;
    }
  }

  return SCL_OK;
}

static scl_error_t scl_ml_kmeans_lloyd(scl_ml_kmeans_t *model,
                                       const SCL_ML_FLOAT *data,
                                       size_t row_stride) {
  size_t n = model->n_samples;
  size_t d = model->n_features;
  size_t k = model->n_clusters;
  float tol = (float)model->params.tol;

  /* Scratch lives on the model: no per-iteration malloc/free. */
  float *dists = model->dists;
  SCL_ML_FLOAT *old_centroids = model->old_centroids;
  SCL_ML_FLOAT *centroids = model->centroids;
  int *labels = model->labels;
  size_t *counts = model->counts;

  for (size_t iter = 0; iter < model->params.max_iter; iter++) {
    /* Assignment: for each point, pick nearest centroid via SIMD
     * squared-distance. Centroids are row-major → contiguous d-vectors. */
    for (size_t ci = 0; ci < n; ci++) {
      const float *x = (const float *)(data + ci * row_stride);
      for (size_t cj = 0; cj < k; cj++)
        dists[cj] =
            scl_ml_simd.dist_l2_sq(x, (const float *)(centroids + cj * d), d);
      size_t best = 0;
      for (size_t cj = 1; cj < k; cj++)
        if (dists[cj] < dists[best])
          best = cj;
      labels[ci] = (int)best;
    }

    memcpy(old_centroids, centroids, k * d * sizeof(SCL_ML_FLOAT));
    memset(centroids, 0, k * d * sizeof(SCL_ML_FLOAT));
    memset(counts, 0, k * sizeof(size_t));

    /* Accumulate sums via SIMD axpy (y += 1.0 * x) per assigned centroid */
    for (size_t ci = 0; ci < n; ci++) {
      int cj = labels[ci];
      if (scl_unlikely(cj < 0 || (size_t)cj >= k))
        continue;
      counts[cj]++;
      scl_ml_simd.axpy((float *)(centroids + cj * d), 1.0f,
                       (const float *)(data + ci * row_stride), d);
    }

    /* Means via SIMD scalar multiply */
    for (size_t cj = 0; cj < k; cj++) {
      if (counts[cj] > 0) {
        float inv = 1.0f / (float)counts[cj];
        scl_ml_simd.mul_s((float *)(centroids + cj * d),
                          (const float *)(centroids + cj * d), inv, d);
      }
    }

    /* Centroid shift (k*d, once per iter — scalar is fine) */
    double shift = 0.0;
    for (size_t i = 0; i < k * d; i++) {
      float diff = (float)(centroids[i] - old_centroids[i]);
      shift += (double)(diff * diff);
    }

    if (shift < (double)tol)
      break;
  }

  /* Final inertia w.r.t. the converged centroids, SIMD distances */
  model->inertia = 0.0f;
  for (size_t ci = 0; ci < n; ci++) {
    int cj = labels[ci];
    if (scl_unlikely(cj < 0 || (size_t)cj >= k))
      continue;
    model->inertia +=
        scl_ml_simd.dist_l2_sq((const float *)(data + ci * row_stride),
                               (const float *)(centroids + cj * d), d);
  }

  return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t scl_ml_kmeans_new(scl_ml_kmeans_t **model,
                                              scl_ml_kmeans_params_t params) {
  if (scl_unlikely(!model))
    return SCL_ERR_NULL_PTR;
  if (!params.alloc)
    return SCL_ERR_INVALID_ARG;
  scl_allocator_t *alloc = params.alloc;
  scl_ml_kmeans_t *m = (scl_ml_kmeans_t *)scl_calloc(
      alloc, 1, sizeof(scl_ml_kmeans_t), SCL_ML_ALIGNMENT);
  if (scl_unlikely(!m))
    return SCL_ERR_OUT_OF_MEMORY;

  m->scratch = scl_alloc_arena_create(alloc, 8192, 0, 0);
  if (!m->scratch) {
    scl_free(alloc, m);
    return SCL_ERR_OUT_OF_MEMORY;
  }

  if (params.n_clusters == 0)
    params.n_clusters = 8;
  if (params.max_iter == 0)
    params.max_iter = 300;
  if (params.n_init == 0)
    params.n_init = 10;

  m->params = params;
  m->alloc = alloc;
  *model = m;
  return SCL_OK;
}

void scl_ml_kmeans_free(scl_ml_kmeans_t *model) {
  if (scl_unlikely(!model))
    return;
  scl_allocator_t *a = model->alloc;
  scl_free(a, model->centroids);
  scl_free(a, model->labels);
  scl_free(a, model->counts);
  scl_free(a, model->old_centroids);
  scl_free(a, model->dists);
  if (model->scratch)
    scl_alloc_arena_destroy(model->scratch);
  memset(model, 0, sizeof(*model));
  scl_free(a, model);
}

SCL_WARN_UNUSED scl_error_t scl_ml_kmeans_fit(scl_ml_kmeans_t *model,
                                              const scl_ml_dataset_t *ds) {
  if (scl_unlikely(!model || !ds))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(!ds->data || ds->n_rows == 0))
    return SCL_ERR_INVALID_ARG;
  if (scl_unlikely(scl_ml_dataset_has_missing(ds)))
    return SCL_ERR_ML_MISSING_DATA;

  scl_ml_simd_init();

  size_t n = ds->n_rows;
  size_t d = ds->n_cols;
  size_t k = model->params.n_clusters;

  if (k > n)
    k = n;
  if (k == 0)
    return SCL_ERR_INVALID_ARG;

  scl_alloc_arena_reset(model->scratch);

  scl_allocator_t *a = model->alloc;
  scl_free(a, model->centroids);
  scl_free(a, model->labels);
  scl_free(a, model->counts);
  scl_free(a, model->old_centroids);
  scl_free(a, model->dists);

  model->n_samples = n;
  model->n_features = d;
  model->n_clusters = k;

  model->centroids = (SCL_ML_FLOAT *)scl_calloc(a, d * k, sizeof(SCL_ML_FLOAT),
                                                SCL_ML_ALIGNMENT);
  model->labels = (int *)scl_calloc(a, n, sizeof(int), SCL_ML_ALIGNMENT);
  model->counts = (size_t *)scl_calloc(a, k, sizeof(size_t), SCL_ML_ALIGNMENT);
  model->old_centroids = (SCL_ML_FLOAT *)scl_alloc(
      a, d * k * sizeof(SCL_ML_FLOAT), SCL_ML_ALIGNMENT);
  model->dists = (float *)scl_alloc(a, k * sizeof(float), SCL_ML_ALIGNMENT);
  if (!model->centroids || !model->labels || !model->counts ||
      !model->old_centroids || !model->dists) {
    scl_free(a, model->centroids);
    scl_free(a, model->labels);
    scl_free(a, model->counts);
    scl_free(a, model->old_centroids);
    scl_free(a, model->dists);
    model->centroids = NULL;
    model->labels = NULL;
    model->counts = NULL;
    model->old_centroids = NULL;
    model->dists = NULL;
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
  int *best_labels = NULL;
  SCL_ML_FLOAT best_inertia = FLT_MAX;

  for (int init = 0; init < model->params.n_init; init++) {
    scl_error_t err =
        scl_ml_kmeans_init_pp(model, ds->data, ds->row_stride, &rng);
    if (err != SCL_OK)
      return err;

    err = scl_ml_kmeans_lloyd(model, ds->data, ds->row_stride);
    if (err != SCL_OK)
      return err;

    if (model->inertia < best_inertia || init == 0) {
      best_inertia = model->inertia;
      if (!best_centroids) {
        best_centroids = (SCL_ML_FLOAT *)scl_alloc(
            model->scratch, d * k * sizeof(SCL_ML_FLOAT), SCL_ML_ALIGNMENT);
        best_labels =
            (int *)scl_alloc(model->scratch, n * sizeof(int), SCL_ML_ALIGNMENT);
        if (!best_centroids || !best_labels) {
          return SCL_ERR_OUT_OF_MEMORY;
        }
      }
      memcpy(best_centroids, model->centroids, d * k * sizeof(SCL_ML_FLOAT));
      memcpy(best_labels, model->labels, n * sizeof(int));
    }
  }

  if (best_centroids) {
    memcpy(model->centroids, best_centroids, d * k * sizeof(SCL_ML_FLOAT));
    memcpy(model->labels, best_labels, n * sizeof(int));
    model->inertia = best_inertia;
  }

  model->fitted = 1;
  return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t scl_ml_kmeans_predict(scl_ml_kmeans_t *model,
                                                  const scl_ml_dataset_t *ds,
                                                  int *y_out) {
  if (scl_unlikely(!model || !ds || !y_out))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(!model->fitted))
    return SCL_ERR_INVALID_STATE;
  if (scl_unlikely(ds->n_cols != model->n_features))
    return SCL_ERR_INVALID_ARG;

  size_t n = ds->n_rows;
  size_t d = model->n_features;
  size_t k = model->n_clusters;

  scl_alloc_arena_reset(model->scratch);

  /* Reuse fit-time scratch dists[] so predict is allocation-free */
  float *dists = model->dists;
  if (scl_unlikely(!dists)) {
    dists =
        (float *)scl_alloc(model->scratch, k * sizeof(float), SCL_ML_ALIGNMENT);
    if (!dists)
      return SCL_ERR_OUT_OF_MEMORY;
  }

  for (size_t ci = 0; ci < n; ci++) {
    const float *x = (const float *)(ds->data + ci * ds->row_stride);
    for (size_t cj = 0; cj < k; cj++)
      dists[cj] = scl_ml_simd.dist_l2_sq(
          x, (const float *)(model->centroids + cj * d), d);
    size_t best = 0;
    for (size_t cj = 1; cj < k; cj++)
      if (dists[cj] < dists[best])
        best = cj;
    y_out[ci] = (int)best;
  }

  return SCL_OK;
}

SCL_PURE size_t scl_ml_kmeans_get_n_clusters(const scl_ml_kmeans_t *model) {
  return model ? model->n_clusters : 0;
}

SCL_PURE const int *scl_ml_kmeans_get_labels(const scl_ml_kmeans_t *model) {
  return model ? model->labels : NULL;
}

SCL_PURE SCL_ML_FLOAT scl_ml_kmeans_get_inertia(const scl_ml_kmeans_t *model) {
  return model ? model->inertia : 0.0f;
}

SCL_WARN_UNUSED scl_error_t scl_ml_kmeans_save(const scl_ml_kmeans_t *model,
                                               uint8_t **buf, size_t *len,
                                               scl_allocator_t *alloc) {
  if (scl_unlikely(!model || !buf || !len))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(!model->fitted))
    return SCL_ERR_INVALID_STATE;
  if (scl_unlikely(!alloc))
    return SCL_ERR_NULL_PTR;

  size_t centroids_bytes =
      model->n_features * model->n_clusters * sizeof(SCL_ML_FLOAT);
  size_t labels_bytes = model->n_samples * sizeof(int);
  size_t counts_bytes = model->n_clusters * sizeof(size_t);

  scl_ml_serial_header_t hdr;
  hdr.magic = SCL_ML_MAGIC;
  hdr.version = SCL_ML_FORMAT_VERSION;
  hdr.algo_id = SCL_ML_ALGO_KMEANS;
  hdr.header_sz = sizeof(hdr);
  hdr.reserved = 0;

  size_t payload_sz = sizeof(size_t) * 3 + sizeof(SCL_ML_FLOAT) +
                      centroids_bytes + labels_bytes + counts_bytes;
  size_t total = sizeof(hdr) + payload_sz + sizeof(uint32_t);

  uint8_t *buffer = (uint8_t *)scl_calloc(alloc, 1, total, SCL_ML_ALIGNMENT);
  if (!buffer)
    return SCL_ERR_OUT_OF_MEMORY;

  memcpy(buffer, &hdr, sizeof(hdr));
  size_t off = sizeof(hdr);

  memcpy(buffer + off, &model->n_clusters, sizeof(size_t));
  off += sizeof(size_t);
  memcpy(buffer + off, &model->n_samples, sizeof(size_t));
  off += sizeof(size_t);
  memcpy(buffer + off, &model->n_features, sizeof(size_t));
  off += sizeof(size_t);
  memcpy(buffer + off, &model->inertia, sizeof(SCL_ML_FLOAT));
  off += sizeof(SCL_ML_FLOAT);
  memcpy(buffer + off, model->centroids, centroids_bytes);
  off += centroids_bytes;
  memcpy(buffer + off, model->labels, labels_bytes);
  off += labels_bytes;
  memcpy(buffer + off, model->counts, counts_bytes);
  off += counts_bytes;

  uint32_t crc =
      scl_ml_crc32c(buffer + sizeof(scl_ml_serial_header_t), payload_sz);
  memcpy(buffer + off, &crc, sizeof(crc));

  *buf = buffer;
  *len = total;
  return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t scl_ml_kmeans_load(scl_ml_kmeans_t **model,
                                               const uint8_t *buf, size_t len,
                                               scl_ml_kmeans_params_t params) {
  if (scl_unlikely(!model || !buf))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(len < sizeof(scl_ml_serial_header_t) + sizeof(uint32_t)))
    return SCL_ERR_INVALID_ARG;

  const scl_ml_serial_header_t *hdr = (const scl_ml_serial_header_t *)buf;
  if (scl_unlikely(hdr->magic != SCL_ML_MAGIC ||
                   hdr->algo_id != SCL_ML_ALGO_KMEANS))
    return SCL_ERR_INVALID_ARG;

  /* Verify payload integrity before any allocation/parsing. */
  uint32_t stored_crc = 0;
  memcpy(&stored_crc, buf + len - sizeof(uint32_t), sizeof(uint32_t));
  uint32_t expected_crc =
      scl_ml_crc32c(buf + sizeof(scl_ml_serial_header_t),
                    len - sizeof(scl_ml_serial_header_t) - sizeof(uint32_t));
  if (scl_unlikely(stored_crc != expected_crc))
    return SCL_ERR_INVALID_ARG;

  size_t off = sizeof(*hdr);
  size_t n_clusters = 0, n_samples = 0, n_features = 0;
  memcpy(&n_clusters, buf + off, sizeof(size_t));
  off += sizeof(size_t);
  memcpy(&n_samples, buf + off, sizeof(size_t));
  off += sizeof(size_t);
  memcpy(&n_features, buf + off, sizeof(size_t));
  off += sizeof(size_t);

  if (params.n_clusters == 0)
    params.n_clusters = n_clusters;
  if (params.max_iter == 0)
    params.max_iter = 300;
  if (params.n_init == 0)
    params.n_init = 10;

  scl_ml_kmeans_t *m;
  scl_error_t nerr = scl_ml_kmeans_new(&m, params);
  if (nerr != SCL_OK)
    return nerr;
  scl_allocator_t *a = m->alloc;
  m->n_clusters = n_clusters;
  m->n_samples = n_samples;
  m->n_features = n_features;

  size_t centroids_bytes = n_features * n_clusters * sizeof(SCL_ML_FLOAT);
  size_t labels_bytes = n_samples * sizeof(int);
  size_t counts_bytes = n_clusters * sizeof(size_t);

  m->centroids =
      (SCL_ML_FLOAT *)scl_calloc(a, 1, centroids_bytes, SCL_ML_ALIGNMENT);
  m->labels = (int *)scl_calloc(a, 1, labels_bytes, SCL_ML_ALIGNMENT);
  m->counts = (size_t *)scl_calloc(a, 1, counts_bytes, SCL_ML_ALIGNMENT);
  if (!m->centroids || !m->labels || !m->counts) {
    scl_free(a, m->centroids);
    scl_free(a, m->labels);
    scl_free(a, m->counts);
    memset(m, 0, sizeof(*m));
    scl_free(a, m);
    return SCL_ERR_OUT_OF_MEMORY;
  }

  memcpy(&m->inertia, buf + off, sizeof(SCL_ML_FLOAT));
  off += sizeof(SCL_ML_FLOAT);
  memcpy(m->centroids, buf + off, centroids_bytes);
  off += centroids_bytes;
  memcpy(m->labels, buf + off, labels_bytes);
  off += labels_bytes;
  memcpy(m->counts, buf + off, counts_bytes);

  m->fitted = 1;
  *model = m;
  return SCL_OK;
}
