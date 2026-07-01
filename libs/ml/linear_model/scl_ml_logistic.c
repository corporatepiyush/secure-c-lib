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

#include "scl_ml_logistic.h"
#include "scl_ml_simd.h"
#include "scl_alloc_arena.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

typedef struct scl_ml_logistic {
    scl_ml_logistic_params_t params;
    SCL_ML_FLOAT *weights;
    SCL_ML_FLOAT  intercept;
    size_t        n_features;
    int           fitted;

    SCL_ML_FLOAT *gradient;
    SCL_ML_FLOAT *pred_buffer;
    scl_allocator_t *alloc;
    scl_allocator_t *scratch;
} scl_ml_logistic_t;

static scl_error_t
scl_ml_logistic_fit_sgd(scl_ml_logistic_t *model, const scl_ml_dataset_t *ds) {
    scl_ml_logistic_params_t *p = &model->params;
    size_t n = ds->n_rows;
    size_t d = ds->n_cols;
    size_t bs = p->batch_size > 0 ? p->batch_size : n;
    float lr = (float)p->learning_rate;
    float alpha_l2 = (float)(p->alpha);
    float tol = (float)p->tol;
    float prev_loss = FLT_MAX;
    int has_converged = 0;
    size_t min_epochs = p->max_iter < 10 ? 0 : 3;

    for (size_t epoch = 0; epoch < p->max_iter; epoch++) {
        double epoch_loss = 0.0;
        float lr_eff = lr / sqrtf((float)(epoch + 1));

        for (size_t batch_start = 0; batch_start < n; batch_start += bs) {
            size_t bsize = bs;
            if (batch_start + bsize > n) bsize = n - batch_start;

            scl_ml_simd.gemv(model->pred_buffer,
                             ds->data + batch_start * ds->row_stride,
                             model->weights, bsize, d, 0.0f);

            for (size_t i = 0; i < bsize; i++)
                model->pred_buffer[i] = scl_ml_clamp_logit(
                    model->pred_buffer[i] + model->intercept);

            scl_ml_simd.sigmoid(model->pred_buffer, model->pred_buffer, bsize);

            double batch_loss = 0.0;
            for (size_t i = 0; i < bsize; i++) {
                size_t row = batch_start + i;
                float p_val = model->pred_buffer[i];
                float t = ds->targets[row];
                model->pred_buffer[i] = p_val - t;
                if (t > 0.5f)
                    batch_loss -= (double)logf((float)SCL_ML_EPSILON + p_val);
                else
                    batch_loss -= (double)logf((float)SCL_ML_EPSILON + 1.0f - p_val);
            }
            epoch_loss += batch_loss;

            memset(model->gradient, 0, d * sizeof(SCL_ML_FLOAT));
            float scale = 1.0f / (float)bsize;
            scl_ml_simd.gemv_t(model->gradient,
                               ds->data + batch_start * ds->row_stride,
                               model->pred_buffer, bsize, d, 0.0f);

            /* Fuse L2 + weight update into a single pass over d.
             * g_j = grad_j*scale + 2*alpha_l2*w_j ;  w_j -= lr_eff * g_j */
            for (size_t j = 0; j < d; j++) {
                float g = model->gradient[j] * scale +
                          2.0f * alpha_l2 * model->weights[j];
                model->gradient[j] = g;
                model->weights[j] -= lr_eff * g;
            }

            float grad_b = 0.0f;
            for (size_t i = 0; i < bsize; i++)
                grad_b += model->pred_buffer[i];
            model->intercept -= lr_eff * grad_b * scale;
        }

        if (epoch >= min_epochs) {
            float loss = (float)(epoch_loss / (double)n);
            if (fabsf(prev_loss - loss) < tol * (fabsf(prev_loss) + 1.0f)) {
                has_converged = 1;
                if (p->verbose)
                    fprintf(stderr, "  Logistic SGD converged at epoch %zu, loss=%g\n",
                            epoch, loss);
                break;
            }
            prev_loss = loss;
        } else {
            prev_loss = (float)(epoch_loss / (double)n);
        }
    }

    if (!has_converged && p->verbose)
        fprintf(stderr, "  Logistic SGD did NOT converge within %zu iterations\n",
                p->max_iter);

    return has_converged ? SCL_OK : SCL_ERR_ML_CONVERGENCE;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_logistic_new(scl_ml_logistic_t **model, scl_ml_logistic_params_t params) {
    if (scl_unlikely(!model)) return SCL_ERR_NULL_PTR;
    if (!params.alloc) return SCL_ERR_INVALID_ARG;
    scl_allocator_t *alloc = params.alloc;
    scl_ml_logistic_t *m = (scl_ml_logistic_t *)scl_calloc(alloc, 1, sizeof(scl_ml_logistic_t), alignof(max_align_t));
    if (scl_unlikely(!m)) return SCL_ERR_OUT_OF_MEMORY;

    m->params = params;
    m->alloc = alloc;
    m->scratch = scl_alloc_arena_create(alloc, 8192, 0);
    if (!m->scratch) { scl_free(alloc, m); return SCL_ERR_OUT_OF_MEMORY; }
    *model = m;
    return SCL_OK;
}

void
scl_ml_logistic_free(scl_ml_logistic_t *model) {
    if (scl_unlikely(!model)) return;
    scl_allocator_t *a = model->alloc;
    scl_free(a, model->weights);
    scl_free(a, model->gradient);
    scl_free(a, model->pred_buffer);
    if (model->scratch) scl_alloc_arena_destroy(model->scratch);
    memset(model, 0, sizeof(*model));
    scl_free(a, model);
}

SCL_WARN_UNUSED scl_error_t
scl_ml_logistic_fit(scl_ml_logistic_t *model, const scl_ml_dataset_t *ds) {
    if (scl_unlikely(!model || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!ds->data || ds->n_rows == 0)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(scl_ml_dataset_has_missing(ds))) return SCL_ERR_ML_MISSING_DATA;

    scl_ml_simd_init();

    scl_alloc_arena_reset(model->scratch);
    scl_allocator_t *a = model->alloc;

    size_t d = ds->n_cols;
    model->n_features = d;

    scl_free(a, model->weights);
    model->weights = (SCL_ML_FLOAT *)scl_calloc(a, d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (scl_unlikely(!model->weights)) return SCL_ERR_OUT_OF_MEMORY;

    size_t bs = model->params.batch_size > 0 ? model->params.batch_size : ds->n_rows;
    scl_free(a, model->gradient);
    scl_free(a, model->pred_buffer);
    model->gradient    = (SCL_ML_FLOAT *)scl_calloc(a, d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    model->pred_buffer = (SCL_ML_FLOAT *)scl_calloc(a, bs, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (!model->gradient || !model->pred_buffer) return SCL_ERR_OUT_OF_MEMORY;

    model->intercept = 0.0f;
    model->fitted = 0;

    scl_error_t err = scl_ml_logistic_fit_sgd(model, ds);
    if (err == SCL_OK) model->fitted = 1;
    return err;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_logistic_predict(scl_ml_logistic_t *model, const scl_ml_dataset_t *ds,
                         SCL_ML_FLOAT *y_out) {
    if (scl_unlikely(!model || !ds || !y_out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(ds->n_cols != model->n_features)) return SCL_ERR_INVALID_ARG;

    size_t n = ds->n_rows;
    size_t d = model->n_features;
    if (n == 0) return SCL_OK;

    /* Per-row SIMD dot (row-stride aware). NOTE: we cannot use gemv here
     * because ds->row_stride may be padded past n_cols by dataset_init, and
     * gemv assumes a packed [m x n] layout.  sigmoid(x) > 0.5 <=> x > 0, so
     * the hard label needs no exp. */
    for (size_t i = 0; i < n; i++) {
        float l = scl_ml_clamp_logit(
            scl_ml_simd.dot_f(&ds->data[i * ds->row_stride], model->weights, d) +
            (float)model->intercept);
        y_out[i] = (l > 0.0f) ? 1.0f : 0.0f;
    }

    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_logistic_predict_proba(scl_ml_logistic_t *model, const scl_ml_dataset_t *ds,
                                SCL_ML_FLOAT *proba_out) {
    if (scl_unlikely(!model || !ds || !proba_out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(ds->n_cols != model->n_features)) return SCL_ERR_INVALID_ARG;

    size_t n = ds->n_rows;
    size_t d = model->n_features;
    if (n == 0) return SCL_OK;

    /* Per-row SIMD dot (row-stride aware) + intercept, then vectorized sigmoid
     * over the contiguous proba_out[] buffer. */
    for (size_t i = 0; i < n; i++)
        proba_out[i] = scl_ml_clamp_logit(
            scl_ml_simd.dot_f(&ds->data[i * ds->row_stride], model->weights, d) +
            (float)model->intercept);
    scl_ml_simd.sigmoid(proba_out, proba_out, n);

    return SCL_OK;
}

SCL_WARN_UNUSED const SCL_ML_FLOAT *
scl_ml_logistic_get_coef(const scl_ml_logistic_t *model) {
    return model ? model->weights : NULL;
}

SCL_WARN_UNUSED SCL_ML_FLOAT
scl_ml_logistic_get_intercept(const scl_ml_logistic_t *model) {
    return model ? model->intercept : 0.0f;
}

SCL_PURE size_t
scl_ml_logistic_get_n_features(const scl_ml_logistic_t *model) {
    return model ? model->n_features : 0;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_logistic_save(const scl_ml_logistic_t *model,
                      uint8_t **buf, size_t *len, scl_allocator_t *alloc) {
    if (scl_unlikely(!model || !buf || !len)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(!alloc)) return SCL_ERR_NULL_PTR;

    size_t weights_bytes = model->n_features * sizeof(SCL_ML_FLOAT);
    scl_ml_serial_header_t hdr;
    hdr.magic     = SCL_ML_MAGIC;
    hdr.version   = SCL_ML_FORMAT_VERSION;
    hdr.algo_id   = SCL_ML_ALGO_LOGISTIC;
    hdr.header_sz = sizeof(hdr);
    hdr.reserved  = 0;
    hdr.crc32c    = 0;

    size_t payload_sz = sizeof(size_t) + sizeof(SCL_ML_FLOAT) + weights_bytes;
    size_t total = sizeof(hdr) + payload_sz + sizeof(uint32_t);

    uint8_t *buffer = (uint8_t *)scl_calloc(alloc, 1, total, alignof(max_align_t));
    if (!buffer) return SCL_ERR_OUT_OF_MEMORY;

    memcpy(buffer, &hdr, sizeof(hdr));
    size_t off = sizeof(hdr);

    memcpy(buffer + off, &model->n_features, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->intercept, sizeof(SCL_ML_FLOAT)); off += sizeof(SCL_ML_FLOAT);
    memcpy(buffer + off, model->weights, weights_bytes); off += weights_bytes;

    uint32_t crc = scl_ml_crc32c(buffer + sizeof(hdr), payload_sz);
    memcpy(buffer + off, &crc, sizeof(crc));

    *buf = buffer;
    *len = total;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_logistic_load(scl_ml_logistic_t **model,
                      const uint8_t *buf, size_t len,
                      scl_ml_logistic_params_t params) {
    if (scl_unlikely(!model || !buf)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(len < sizeof(scl_ml_serial_header_t) + sizeof(uint32_t)))
        return SCL_ERR_INVALID_ARG;

    const scl_ml_serial_header_t *hdr = (const scl_ml_serial_header_t *)buf;
    if (scl_unlikely(hdr->magic != SCL_ML_MAGIC || hdr->algo_id != SCL_ML_ALGO_LOGISTIC))
        return SCL_ERR_INVALID_ARG;

    size_t off = sizeof(*hdr);
    size_t n_features = 0;
    memcpy(&n_features, buf + off, sizeof(size_t)); off += sizeof(size_t);

    if (n_features == 0 || n_features > len) return SCL_ERR_INVALID_ARG;

    scl_ml_logistic_t *m;
    scl_error_t err = scl_ml_logistic_new(&m, params);
    if (err != SCL_OK) return err;

    scl_allocator_t *a = m->alloc;
    m->n_features = n_features;

    size_t weights_bytes = n_features * sizeof(SCL_ML_FLOAT);
    if (off + sizeof(SCL_ML_FLOAT) + weights_bytes > len) {
        scl_ml_logistic_free(m);
        return SCL_ERR_INVALID_ARG;
    }

    m->weights = (SCL_ML_FLOAT *)scl_calloc(a, n_features, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (!m->weights) { scl_ml_logistic_free(m); return SCL_ERR_OUT_OF_MEMORY; }

    memcpy(&m->intercept, buf + off, sizeof(SCL_ML_FLOAT)); off += sizeof(SCL_ML_FLOAT);
    memcpy(m->weights, buf + off, weights_bytes);

    uint32_t stored_crc = 0;
    memcpy(&stored_crc, buf + len - sizeof(uint32_t), sizeof(uint32_t));
    uint32_t expected_crc = scl_ml_crc32c(buf + sizeof(*hdr), len - sizeof(*hdr) - sizeof(uint32_t));
    if (stored_crc != expected_crc) {
        scl_ml_logistic_free(m);
        return SCL_ERR_INVALID_ARG;
    }

    m->fitted = 1;
    *model = m;
    return SCL_OK;
}
