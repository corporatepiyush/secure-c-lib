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

#include "scl_ml_gmm.h"
#include "scl_ml_simd.h"
#include "scl_alloc_arena.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

#define SCL_GMM_LOG_2PI 1.8378770664093453f

typedef struct scl_ml_gmm {
    scl_ml_gmm_params_t params;
    SCL_ML_FLOAT *weights;
    SCL_ML_FLOAT *means;
    SCL_ML_FLOAT *covariances;
    SCL_ML_FLOAT *precisions;
    SCL_ML_FLOAT *log_covar_det;
    SCL_ML_FLOAT *responsibilities;
    size_t        n_components;
    size_t        n_features;
    SCL_ML_FLOAT  log_likelihood_;
    int           fitted;
    scl_allocator_t *alloc;
    scl_allocator_t *scratch;
} scl_ml_gmm_t;

/* Deterministic, thread-safe local PRNG (xorshift32) — replaces the global
 * srand()/rand() pair which is unsafe under concurrent use and not seedable
 * per-instance. */
static inline uint32_t
scl_ml_gmm_next(uint32_t *st)
{
    uint32_t x = *st;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *st = x;
    return x;
}

static inline double
scl_ml_gmm_uniform(uint32_t *st)
{
    return (double)(scl_ml_gmm_next(st) >> 8) / (double)(1u << 24);
}

static scl_error_t
scl_ml_gmm_init_kmeans(scl_ml_gmm_t *model, const scl_ml_dataset_t *ds) {
    size_t n = ds->n_rows;
    size_t d = ds->n_cols;
    size_t K = model->n_components;
    size_t stride = ds->row_stride;

    uint32_t rng = (uint32_t)model->params.random_seed;
    if (rng == 0) rng = 0x9E3779B9u;

    size_t first = (size_t)(scl_ml_gmm_uniform(&rng) * (double)n);
    if (first >= n) first = n - 1;
    for (size_t j = 0; j < d; j++)
        model->means[j] = ds->data[first * stride + j];

    SCL_ML_FLOAT *min_dist = (SCL_ML_FLOAT *)scl_alloc(model->scratch, n * sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (!min_dist) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t k = 1; k < K; k++) {
        double total = 0.0;
        for (size_t i = 0; i < n; i++) {
            double best = DBL_MAX;
            for (size_t kk = 0; kk < k; kk++) {
                double dsum = 0.0;
                for (size_t j = 0; j < d; j++) {
                    double diff = (double)ds->data[i * stride + j] -
                                  (double)model->means[kk * d + j];
                    dsum += diff * diff;
                }
                if (dsum < best) best = dsum;
            }
            min_dist[i] = (SCL_ML_FLOAT)best;
            total += best;
        }

        double sample = scl_ml_gmm_uniform(&rng) * total;
        double cum = 0.0;
        size_t chosen = n - 1;
        for (size_t i = 0; i < n; i++) {
            cum += (double)min_dist[i];
            if (cum >= sample) { chosen = i; break; }
        }

        for (size_t j = 0; j < d; j++)
            model->means[k * d + j] = ds->data[chosen * stride + j];
    }

    SCL_ML_FLOAT inv_K = 1.0f / (SCL_ML_FLOAT)K;
    for (size_t k = 0; k < K; k++)
        model->weights[k] = inv_K;

    SCL_ML_FLOAT *global_var = (SCL_ML_FLOAT *)scl_calloc(model->scratch, d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (!global_var) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t j = 0; j < d; j++) {
        double m = 0.0;
        for (size_t i = 0; i < n; i++)
            m += (double)ds->data[i * stride + j];
        m /= (double)n;
        double v = 0.0;
        for (size_t i = 0; i < n; i++) {
            double diff = (double)ds->data[i * stride + j] - m;
            v += diff * diff;
        }
        v /= (double)n;
        global_var[j] = (SCL_ML_FLOAT)v;
    }

    for (size_t k = 0; k < K; k++) {
        double sum_log = 0.0;
        for (size_t j = 0; j < d; j++) {
            model->covariances[k * d + j] =
                global_var[j] + (SCL_ML_FLOAT)model->params.reg_covar;
            model->precisions[k * d + j] =
                1.0f / model->covariances[k * d + j];
            sum_log += (double)logf(model->covariances[k * d + j]);
        }
        model->log_covar_det[k] = (SCL_ML_FLOAT)sum_log;
    }

    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_gmm_new(scl_ml_gmm_t **model, scl_ml_gmm_params_t params) {
    if (scl_unlikely(!model)) return SCL_ERR_NULL_PTR;
    if (!params.alloc) return SCL_ERR_INVALID_ARG;
    scl_allocator_t *alloc = params.alloc;
    scl_ml_gmm_t *m = (scl_ml_gmm_t *)scl_calloc(
        alloc, 1, sizeof(scl_ml_gmm_t), alignof(max_align_t));
    if (scl_unlikely(!m)) return SCL_ERR_OUT_OF_MEMORY;

    m->scratch = scl_alloc_arena_create(alloc, 8192, 0);
    if (!m->scratch) { scl_free(alloc, m); return SCL_ERR_OUT_OF_MEMORY; }

    m->params = params;
    m->alloc  = alloc;
    if (params.n_components == 0)
        m->n_components = 1;
    else
        m->n_components = params.n_components;

    *model = m;
    return SCL_OK;
}

void
scl_ml_gmm_free(scl_ml_gmm_t *model) {
    if (scl_unlikely(!model)) return;
    scl_allocator_t *a = model->alloc;
    scl_free(a, model->weights);
    scl_free(a, model->means);
    scl_free(a, model->covariances);
    scl_free(a, model->precisions);
    scl_free(a, model->log_covar_det);
    scl_free(a, model->responsibilities);
    if (model->scratch) scl_alloc_arena_destroy(model->scratch);
    memset(model, 0, sizeof(*model));
    scl_free(a, model);
}

SCL_WARN_UNUSED scl_error_t
scl_ml_gmm_fit(scl_ml_gmm_t *model, const scl_ml_dataset_t *ds) {
    if (scl_unlikely(!model || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!ds->data || ds->n_rows == 0)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(scl_ml_dataset_has_missing(ds))) return SCL_ERR_ML_MISSING_DATA;

    scl_ml_simd_init();

    size_t n = ds->n_rows;
    size_t d = ds->n_cols;
    size_t K = model->n_components;
    size_t stride = ds->row_stride;

    if (K > n) K = n;
    if (K == 0) return SCL_ERR_INVALID_ARG;

    scl_alloc_arena_reset(model->scratch);

    model->n_features = d;
    model->n_components = K;

    size_t resp_sz = n * K;

    scl_allocator_t *a = model->alloc;

    scl_free(a, model->weights);
    scl_free(a, model->means);
    scl_free(a, model->covariances);
    scl_free(a, model->precisions);
    scl_free(a, model->log_covar_det);
    scl_free(a, model->responsibilities);

    model->weights = (SCL_ML_FLOAT *)scl_calloc(a, K, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    model->means = (SCL_ML_FLOAT *)scl_calloc(a, K * d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    model->covariances = (SCL_ML_FLOAT *)scl_calloc(a, K * d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    model->precisions = (SCL_ML_FLOAT *)scl_calloc(a, K * d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    model->log_covar_det = (SCL_ML_FLOAT *)scl_calloc(a, K, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    model->responsibilities = (SCL_ML_FLOAT *)scl_calloc(a, resp_sz, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (!model->weights || !model->means || !model->covariances ||
        !model->precisions || !model->log_covar_det || !model->responsibilities) {
        return SCL_ERR_OUT_OF_MEMORY;
    }

    scl_error_t err = scl_ml_gmm_init_kmeans(model, ds);
    if (scl_unlikely(err != SCL_OK)) return err;

    SCL_ML_FLOAT *sum_x = (SCL_ML_FLOAT *)scl_calloc(model->scratch, K * d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    double *N_k = (double *)scl_calloc(model->scratch, K, sizeof(double), alignof(max_align_t));
    if (!sum_x || !N_k) { return SCL_ERR_OUT_OF_MEMORY; }

    double prev_log_likelihood = -DBL_MAX;
    int converged = 0;
    size_t max_iter = model->params.max_iter;
    float reg = (float)model->params.reg_covar;

    for (size_t iter = 0; iter < max_iter; iter++) {
        double log_likelihood = 0.0;

        for (size_t i = 0; i < n; i++) {
            double max_log = -DBL_MAX;

            for (size_t k = 0; k < K; k++) {
                if (model->weights[k] <= 0.0f) {
                    model->responsibilities[i * K + k] = (float)-DBL_MAX;
                    continue;
                }
                double quad = 0.0;
                for (size_t j = 0; j < d; j++) {
                    double diff = (double)ds->data[i * stride + j] -
                                  (double)model->means[k * d + j];
                    quad += diff * diff * (double)model->precisions[k * d + j];
                }
                double log_prob = -0.5 * ((double)d * (double)SCL_GMM_LOG_2PI +
                    (double)model->log_covar_det[k] + quad);
                double log_resp_val = log((double)model->weights[k]) + log_prob;
                model->responsibilities[i * K + k] = (SCL_ML_FLOAT)log_resp_val;
                if (log_resp_val > max_log) max_log = log_resp_val;
            }

            double sum_exp = 0.0;
            for (size_t k = 0; k < K; k++) {
                double val = exp((double)model->responsibilities[i * K + k] - max_log);
                model->responsibilities[i * K + k] = (SCL_ML_FLOAT)val;
                sum_exp += val;
            }

            double inv_sum = 1.0 / sum_exp;
            for (size_t k = 0; k < K; k++)
                model->responsibilities[i * K + k] *= (SCL_ML_FLOAT)inv_sum;

            log_likelihood += max_log + log(sum_exp);
        }

        if (iter > 0) {
            double change = fabs(log_likelihood - prev_log_likelihood);
            if (change < model->params.tol * (fabs(prev_log_likelihood) + 1.0)) {
                converged = 1;
            }
        }
        prev_log_likelihood = log_likelihood;

        if (converged)
            break;

        memset(sum_x, 0, K * d * sizeof(SCL_ML_FLOAT));
        memset(N_k, 0, K * sizeof(double));

        for (size_t i = 0; i < n; i++) {
            for (size_t k = 0; k < K; k++) {
                double resp = (double)model->responsibilities[i * K + k];
                if (resp <= 0.0) continue;
                N_k[k] += resp;
                for (size_t j = 0; j < d; j++)
                    sum_x[k * d + j] +=
                        (SCL_ML_FLOAT)(resp * (double)ds->data[i * stride + j]);
            }
        }

        for (size_t k = 0; k < K; k++) {
            if (N_k[k] > 0.0) {
                double inv_nk = 1.0 / N_k[k];
                for (size_t j = 0; j < d; j++)
                    model->means[k * d + j] =
                        (SCL_ML_FLOAT)((double)sum_x[k * d + j] * inv_nk);
            }
            model->weights[k] = (SCL_ML_FLOAT)(N_k[k] / (double)n);
        }

        memset(sum_x, 0, K * d * sizeof(SCL_ML_FLOAT));

        for (size_t i = 0; i < n; i++) {
            for (size_t k = 0; k < K; k++) {
                double resp = (double)model->responsibilities[i * K + k];
                if (resp <= 0.0) continue;
                for (size_t j = 0; j < d; j++) {
                    double diff = (double)ds->data[i * stride + j] -
                                  (double)model->means[k * d + j];
                    sum_x[k * d + j] += (SCL_ML_FLOAT)(resp * diff * diff);
                }
            }
        }

        for (size_t k = 0; k < K; k++) {
            if (N_k[k] > 0.0) {
                double inv_nk = 1.0 / N_k[k];
                double sum_log = 0.0;
                for (size_t j = 0; j < d; j++) {
                    model->covariances[k * d + j] =
                        (SCL_ML_FLOAT)((double)sum_x[k * d + j] * inv_nk) + reg;
                    model->precisions[k * d + j] =
                        1.0f / model->covariances[k * d + j];
                    sum_log += (double)logf(model->covariances[k * d + j]);
                }
                model->log_covar_det[k] = (SCL_ML_FLOAT)sum_log;
            }
        }
    }

    model->log_likelihood_ = (SCL_ML_FLOAT)prev_log_likelihood;
    model->fitted = 1;
    return converged ? SCL_OK : SCL_ERR_ML_CONVERGENCE;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_gmm_predict(scl_ml_gmm_t *model, const scl_ml_dataset_t *ds,
                    size_t *labels_out) {
    if (scl_unlikely(!model || !ds || !labels_out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(ds->n_cols != model->n_features)) return SCL_ERR_INVALID_ARG;

    size_t n = ds->n_rows;
    size_t d = model->n_features;
    size_t K = model->n_components;
    size_t stride = ds->row_stride;

    scl_alloc_arena_reset(model->scratch);

    for (size_t i = 0; i < n; i++) {
        double best = -DBL_MAX;
        size_t best_k = 0;
        for (size_t k = 0; k < K; k++) {
            if (model->weights[k] <= 0.0f) continue;
            double quad = 0.0;
            for (size_t j = 0; j < d; j++) {
                double diff = (double)ds->data[i * stride + j] -
                              (double)model->means[k * d + j];
                quad += diff * diff * (double)model->precisions[k * d + j];
            }
            double log_resp_val = log((double)model->weights[k]) -
                                  0.5 * ((double)d * (double)SCL_GMM_LOG_2PI +
                                   (double)model->log_covar_det[k] + quad);
            if (log_resp_val > best) {
                best = log_resp_val;
                best_k = k;
            }
        }
        labels_out[i] = best_k;
    }

    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_gmm_predict_proba(scl_ml_gmm_t *model, const scl_ml_dataset_t *ds,
                          SCL_ML_FLOAT *proba_out) {
    if (scl_unlikely(!model || !ds || !proba_out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(ds->n_cols != model->n_features)) return SCL_ERR_INVALID_ARG;

    size_t n = ds->n_rows;
    size_t d = model->n_features;
    size_t K = model->n_components;
    size_t stride = ds->row_stride;

    for (size_t i = 0; i < n; i++) {
        double max_log = -DBL_MAX;
        SCL_ML_FLOAT *row = proba_out + i * K;

        for (size_t k = 0; k < K; k++) {
            if (model->weights[k] <= 0.0f) {
                row[k] = (float)-DBL_MAX;
                continue;
            }
            double quad = 0.0;
            for (size_t j = 0; j < d; j++) {
                double diff = (double)ds->data[i * stride + j] -
                              (double)model->means[k * d + j];
                quad += diff * diff * (double)model->precisions[k * d + j];
            }
            double log_resp_val = log((double)model->weights[k]) -
                                  0.5 * ((double)d * (double)SCL_GMM_LOG_2PI +
                                   (double)model->log_covar_det[k] + quad);
            row[k] = (SCL_ML_FLOAT)log_resp_val;
            if (log_resp_val > max_log) max_log = log_resp_val;
        }

        double sum_exp = 0.0;
        for (size_t k = 0; k < K; k++) {
            double val = exp((double)row[k] - max_log);
            row[k] = (SCL_ML_FLOAT)val;
            sum_exp += val;
        }

        double inv_sum = 1.0 / sum_exp;
        for (size_t k = 0; k < K; k++)
            row[k] *= (SCL_ML_FLOAT)inv_sum;
    }

    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_gmm_save(const scl_ml_gmm_t *model,
                 uint8_t **buf, size_t *len, scl_allocator_t *alloc) {
    if (scl_unlikely(!model || !buf || !len)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(!alloc)) return SCL_ERR_NULL_PTR;

    size_t nc = model->n_components;
    size_t d  = model->n_features;

    scl_ml_serial_header_t hdr;
    hdr.magic     = SCL_ML_MAGIC;
    hdr.version   = SCL_ML_FORMAT_VERSION;
    hdr.algo_id   = SCL_ML_ALGO_GMM;
    hdr.header_sz = sizeof(hdr);
    hdr.reserved  = 0;

    size_t payload_sz = sizeof(size_t) + sizeof(size_t) +
                        nc * sizeof(SCL_ML_FLOAT) +
                        nc * d * sizeof(SCL_ML_FLOAT) +
                        nc * d * sizeof(SCL_ML_FLOAT);
    size_t total = sizeof(hdr) + payload_sz + sizeof(uint32_t);

    uint8_t *buffer = (uint8_t *)scl_calloc(alloc, 1, total, alignof(max_align_t));
    if (!buffer) return SCL_ERR_OUT_OF_MEMORY;

    memcpy(buffer, &hdr, sizeof(hdr));
    size_t off = sizeof(hdr);

    memcpy(buffer + off, &nc, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &d,  sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, model->weights, nc * sizeof(SCL_ML_FLOAT));
    off += nc * sizeof(SCL_ML_FLOAT);
    memcpy(buffer + off, model->means, nc * d * sizeof(SCL_ML_FLOAT));
    off += nc * d * sizeof(SCL_ML_FLOAT);
    memcpy(buffer + off, model->covariances, nc * d * sizeof(SCL_ML_FLOAT));
    off += nc * d * sizeof(SCL_ML_FLOAT);

    uint32_t crc = scl_ml_crc32c(buffer + sizeof(scl_ml_serial_header_t), payload_sz);
    memcpy(buffer + off, &crc, sizeof(crc));

    *buf = buffer;
    *len = total;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_gmm_load(scl_ml_gmm_t **model,
                 const uint8_t *buf, size_t len,
                 scl_ml_gmm_params_t params) {
    if (scl_unlikely(!model || !buf)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(len < sizeof(scl_ml_serial_header_t) + sizeof(uint32_t)))
        return SCL_ERR_INVALID_ARG;

    const scl_ml_serial_header_t *hdr = (const scl_ml_serial_header_t *)buf;
    if (scl_unlikely(hdr->magic != SCL_ML_MAGIC || hdr->algo_id != SCL_ML_ALGO_GMM))
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
    size_t nc = 0, d = 0;
    memcpy(&nc, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&d,  buf + off, sizeof(size_t)); off += sizeof(size_t);

    params.n_components = nc;
    scl_ml_gmm_t *m;
    scl_error_t err = scl_ml_gmm_new(&m, params);
    if (err != SCL_OK) return err;

    m->n_features = d;
    m->n_components = nc;

    scl_allocator_t *a = m->alloc;
    m->weights = (SCL_ML_FLOAT *)scl_calloc(a, nc, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    m->means = (SCL_ML_FLOAT *)scl_calloc(a, nc * d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    m->covariances = (SCL_ML_FLOAT *)scl_calloc(a, nc * d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    m->precisions = (SCL_ML_FLOAT *)scl_calloc(a, nc * d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    m->log_covar_det = (SCL_ML_FLOAT *)scl_calloc(a, nc, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (!m->weights || !m->means || !m->covariances ||
        !m->precisions || !m->log_covar_det) {
        scl_ml_gmm_free(m);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    memcpy(m->weights, buf + off, nc * sizeof(SCL_ML_FLOAT));
    off += nc * sizeof(SCL_ML_FLOAT);
    memcpy(m->means, buf + off, nc * d * sizeof(SCL_ML_FLOAT));
    off += nc * d * sizeof(SCL_ML_FLOAT);
    memcpy(m->covariances, buf + off, nc * d * sizeof(SCL_ML_FLOAT));

    for (size_t k = 0; k < nc; k++) {
        double sum_log = 0.0;
        for (size_t j = 0; j < d; j++) {
            m->precisions[k * d + j] = 1.0f / m->covariances[k * d + j];
            sum_log += (double)logf(m->covariances[k * d + j]);
        }
        m->log_covar_det[k] = (SCL_ML_FLOAT)sum_log;
    }

    m->fitted = 1;
    *model = m;
    return SCL_OK;
}
