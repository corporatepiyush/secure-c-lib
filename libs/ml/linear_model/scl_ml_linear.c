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

#include "scl_ml_linear.h"
#include "scl_ml_simd.h"
#include "scl_alloc_arena.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

/* ── Internal state ──────────────────────────────────────────── */
typedef struct scl_ml_linear {
    scl_ml_linear_params_t params;
    SCL_ML_FLOAT *weights;       /* [n_features], 32-byte aligned */
    SCL_ML_FLOAT  intercept;
    size_t        n_features;
    int           fitted;

    /* Pre-allocated working buffers (fit-time) */
    SCL_ML_FLOAT *gradient;      /* [n_features] */
    SCL_ML_FLOAT *pred_buffer;   /* [batch_size] */
    SCL_ML_FLOAT *resid_buffer;  /* [batch_size] */
    scl_allocator_t *alloc;
    scl_allocator_t *scratch;
} scl_ml_linear_t;

/* ── Helpers ─────────────────────────────────────────────────── */

/* Soft thresholding for L1 (used in CD and ISTA) */
static inline float scl_ml_soft_threshold(float x, float lambda) {
    if (x > lambda) return x - lambda;
    if (x < -lambda) return x + lambda;
    return 0.0f;
}

/* L1 proximal operator */
static inline void scl_ml_prox_l1(SCL_ML_FLOAT *w, size_t n, float alpha_l1) {
    for (size_t i = 0; i < n; i++)
        w[i] = scl_ml_soft_threshold(w[i], alpha_l1);
}

/* ── SGD solver ──────────────────────────────────────────────── */
static scl_error_t scl_ml_linear_fit_sgd(scl_ml_linear_t *model,
                                          const scl_ml_dataset_t *ds) {
    scl_ml_linear_params_t *p = &model->params;
    size_t n = ds->n_rows;
    size_t d = ds->n_cols;
    size_t bs = p->batch_size > 0 ? p->batch_size : n;
    float lr = p->learning_rate > 0.0f ? (float)p->learning_rate :
               1.0f / sqrtf((float)(d + 1));
    float alpha_l2 = (float)(p->alpha * (1.0 - p->l1_ratio));
    float alpha_l1 = (float)(p->alpha * p->l1_ratio);
    float tol = (float)p->tol;

    /* Early stopping: track gradient norm */
    float prev_loss = FLT_MAX;
    int has_converged = 0;

    for (size_t epoch = 0; epoch < p->max_iter; epoch++) {
        /* Shuffle indices — we use simple sequential scan (faster, same convergence)
         * Random shuffling gives faster convergence but costs O(n) shuffle per epoch */
        double epoch_loss = 0.0;

        for (size_t batch_start = 0; batch_start < n; batch_start += bs) {
            size_t bsize = bs;
            if (batch_start + bsize > n) bsize = n - batch_start;

            /* Compute predictions for batch: pred = X[batch] @ w + b */
            for (size_t i = 0; i < bsize; i++) {
                size_t row = batch_start + i;
                double pred = (double)model->intercept;
                for (size_t j = 0; j < d; j++)
                    pred += (double)ds->data[row * ds->row_stride + j] * (double)model->weights[j];
                model->pred_buffer[i] = (float)pred;
            }

            /* Compute residuals and loss */
            double batch_loss = 0.0;
            for (size_t i = 0; i < bsize; i++) {
                size_t row = batch_start + i;
                float resid = model->pred_buffer[i] - ds->targets[row];
                model->resid_buffer[i] = resid;
                batch_loss += (double)(resid * resid);
            }
            epoch_loss += batch_loss;

            /* Compute gradient: grad = (2/bsize) * X^T @ resid */
            memset(model->gradient, 0, d * sizeof(SCL_ML_FLOAT));
            float scale = 2.0f / (float)bsize;
            for (size_t i = 0; i < bsize; i++) {
                size_t row = batch_start + i;
                float r = model->resid_buffer[i] * scale;
                for (size_t j = 0; j < d; j++)
                    model->gradient[j] += r * ds->data[row * ds->row_stride + j];
            }

            /* Add L2 regularization gradient */
            if (alpha_l2 > 0.0f) {
                for (size_t j = 0; j < d; j++)
                    model->gradient[j] += 2.0f * alpha_l2 * model->weights[j];
            }

            /* Update weights: w -= lr * grad */
            float lr_eff = lr;
            /* Learning rate schedule: inverse scaling */
            if (epoch > 0) lr_eff = lr / sqrtf((float)(epoch + 1));

            for (size_t j = 0; j < d; j++)
                model->weights[j] -= lr_eff * model->gradient[j];

            /* Update intercept */
            float grad_b = 0.0f;
            for (size_t i = 0; i < bsize; i++)
                grad_b += model->resid_buffer[i];
            model->intercept -= lr_eff * grad_b * scale;

            /* Proximal step for L1 */
            if (alpha_l1 > 0.0f)
                scl_ml_prox_l1(model->weights, d, lr_eff * alpha_l1);
        }

        /* Check convergence */
        if (epoch > 0) {
            float loss = (float)(epoch_loss / (double)n);
            if (fabsf(prev_loss - loss) < tol * (fabsf(prev_loss) + 1.0f)) {
                has_converged = 1;
                if (p->verbose) fprintf(stderr, "  SGD converged at epoch %zu, loss=%g\n", epoch, loss);
                break;
            }
            prev_loss = loss;
        } else {
            prev_loss = (float)(epoch_loss / (double)n);
        }
    }

    if (!has_converged && p->verbose)
        fprintf(stderr, "  SGD did NOT converge within %zu iterations\n", p->max_iter);

    return has_converged ? SCL_OK : SCL_ERR_ML_CONVERGENCE;
}

/* ── Normal Equations solver (closed-form for OLS/Ridge) ────── */
static scl_error_t scl_ml_linear_fit_normal(scl_ml_linear_t *model,
                                              const scl_ml_dataset_t *ds) {
    size_t n = ds->n_rows;
    size_t d = ds->n_cols;
    float alpha_l2 = (float)(model->params.alpha * (1.0 - model->params.l1_ratio));

    /* Compute column means and center data for correct intercept handling.
     * Solve (X_c^T X_c) @ w = X_c^T y_c  then b = mean_y - mean(X) @ w. */
    double *mean_x = (double *)scl_calloc(model->scratch, d, sizeof(double), alignof(max_align_t));
    double  mean_y = 0.0;
    if (!mean_x) return SCL_ERR_OUT_OF_MEMORY;
    for (size_t i = 0; i < n; i++) {
        mean_y += (double)ds->targets[i];
        for (size_t j = 0; j < d; j++)
            mean_x[j] += (double)ds->data[i * ds->row_stride + j];
    }
    mean_y /= (double)n;
    for (size_t j = 0; j < d; j++)
        mean_x[j] /= (double)n;

    double *XtX = (double *)scl_calloc(model->scratch, d * d, sizeof(double), alignof(max_align_t));
    double *Xty = (double *)scl_calloc(model->scratch, d, sizeof(double), alignof(max_align_t));
    double *xc  = (double *)scl_calloc(model->scratch, d, sizeof(double), alignof(max_align_t));   /* centered row scratch */
    if (!XtX || !Xty || !xc) {
        return SCL_ERR_OUT_OF_MEMORY;
    }

    /* Center each row once (xc[]) instead of re-subtracting mean_x[k] in the
     * inner j,k loop. Reduces subtractions from O(n*d^2) to O(n*d). */
    for (size_t i = 0; i < n; i++) {
        double yc = (double)ds->targets[i] - mean_y;
        for (size_t j = 0; j < d; j++) {
            xc[j] = (double)ds->data[i * ds->row_stride + j] - mean_x[j];
            Xty[j] += xc[j] * yc;
        }
        for (size_t j = 0; j < d; j++) {
            double xcj = xc[j];
            double *row = &XtX[j * d];
            for (size_t k = 0; k < d; k++)
                row[k] += xcj * xc[k];
        }
    }

    /* Add L2 regularization (Ridge) to diagonal */
    if (alpha_l2 > 0.0f) {
        for (size_t j = 0; j < d; j++)
            XtX[j * d + j] += 2.0 * (double)n * (double)alpha_l2;
    }

    /* Solve (X_c^T X_c) * w = X_c^T y_c via Cholesky decomposition */
    double *L = (double *)scl_calloc(model->scratch, d * d, sizeof(double), alignof(max_align_t));
    if (!L) { return SCL_ERR_OUT_OF_MEMORY; }

    for (size_t j = 0; j < d; j++) {
        double sum = 0.0;
        for (size_t k = 0; k < j; k++)
            sum += L[j * d + k] * L[j * d + k];
        double val = XtX[j * d + j] - sum;
        if (scl_unlikely(val <= 0.0)) {
            return SCL_ERR_ML_SINGULAR;
        }
        L[j * d + j] = sqrt(val);
        for (size_t i = j + 1; i < d; i++) {
            double sum2 = 0.0;
            for (size_t k = 0; k < j; k++)
                sum2 += L[i * d + k] * L[j * d + k];
            L[i * d + j] = (XtX[i * d + j] - sum2) / L[j * d + j];
        }
    }

    double *z = (double *)scl_calloc(model->scratch, d, sizeof(double), alignof(max_align_t));
    if (!z) { return SCL_ERR_OUT_OF_MEMORY; }
    for (size_t i = 0; i < d; i++) {
        double sum = 0.0;
        for (size_t j = 0; j < i; j++)
            sum += L[i * d + j] * z[j];
        z[i] = (Xty[i] - sum) / L[i * d + i];
    }

    for (size_t i = d; i > 0; i--) {
        size_t ii = i - 1;
        double sum = 0.0;
        for (size_t j = ii + 1; j < d; j++)
            sum += L[j * d + ii] * (double)model->weights[j];
        model->weights[ii] = (float)((z[ii] - sum) / L[ii * d + ii]);
    }

    model->intercept = (float)(mean_y);
    for (size_t j = 0; j < d; j++)
        model->intercept -= (float)(mean_x[j] * (double)model->weights[j]);

    return SCL_OK;
}

/* ── Coordinate Descent solver (for Lasso / ElasticNet) ─────── */
static scl_error_t scl_ml_linear_fit_cd(scl_ml_linear_t *model,
                                          const scl_ml_dataset_t *ds) {
    size_t n = ds->n_rows;
    size_t d = ds->n_cols;
    float alpha_l1 = (float)(model->params.alpha * model->params.l1_ratio);
    float alpha_l2 = (float)(model->params.alpha * (1.0 - model->params.l1_ratio));
    float tol = (float)model->params.tol;

    /* Pre-compute X columns: col_norms[j] = sum_i X[i][j]^2 */
    float *col_norms = (float *)scl_calloc(model->scratch, d, sizeof(float), alignof(max_align_t));
    if (!col_norms) return SCL_ERR_OUT_OF_MEMORY;
    for (size_t j = 0; j < d; j++)
        for (size_t i = 0; i < n; i++)
            col_norms[j] += ds->data[i * ds->row_stride + j] *
                            ds->data[i * ds->row_stride + j];
    for (size_t j = 0; j < d; j++)
        col_norms[j] += 2.0f * alpha_l2;
    /* For features with zero variance, leave them at 0 */
    for (size_t j = 0; j < d; j++)
        if (col_norms[j] < FLT_MIN) col_norms[j] = 1.0f;

    /* Initial predictions */
    float *pred = (float *)scl_calloc(model->scratch, n, sizeof(float), alignof(max_align_t));
    if (!pred) { return SCL_ERR_OUT_OF_MEMORY; }
    for (size_t i = 0; i < n; i++) pred[i] = model->intercept;

    double prev_loss = 0.0;
    for (size_t epoch = 0; epoch < model->params.max_iter; epoch++) {
        double epoch_loss = 0.0;

        for (size_t j = 0; j < d; j++) {
            /* Compute rho_j = sum_i X[i][j] * (y[i] - pred[i] + w[j] * X[i][j]) */
            float rho = 0.0f;
            for (size_t i = 0; i < n; i++)
                rho += ds->data[i * ds->row_stride + j] *
                       (ds->targets[i] - pred[i] + model->weights[j] * ds->data[i * ds->row_stride + j]);

            float old_w = model->weights[j];
            model->weights[j] = scl_ml_soft_threshold(rho, alpha_l1) / col_norms[j];

            if (fabsf(model->weights[j] - old_w) > FLT_MIN) {
                float delta = model->weights[j] - old_w;
                for (size_t i = 0; i < n; i++)
                    pred[i] += delta * ds->data[i * ds->row_stride + j];
            }
        }

        /* Compute loss for convergence check */
        for (size_t i = 0; i < n; i++) {
            float res = ds->targets[i] - pred[i];
            epoch_loss += (double)(res * res);
        }

        if (epoch > 0) {
            double rel_change = fabs(epoch_loss - prev_loss) / (fabs(prev_loss) + 1.0);
            if (rel_change < (double)tol) {
                return SCL_OK;
            }
        }
        prev_loss = epoch_loss;
    }

    float *resid = (float *)scl_calloc(model->scratch, n, sizeof(float), alignof(max_align_t));
    if (!resid) return SCL_ERR_OUT_OF_MEMORY;
    /* Compute intercept */
    double sum_res = 0.0;
    for (size_t i = 0; i < n; i++) {
        double p = (double)model->intercept;
        for (size_t j = 0; j < d; j++)
            p += (double)ds->data[i * ds->row_stride + j] * (double)model->weights[j];
        sum_res += (double)ds->targets[i] - p;
    }
    model->intercept += (float)(sum_res / (double)n);

    return SCL_OK;
}

/* ── Public API ──────────────────────────────────────────────── */

SCL_WARN_UNUSED scl_error_t
scl_ml_linear_new(scl_ml_linear_t **model, scl_ml_linear_params_t params) {
    if (scl_unlikely(!model)) return SCL_ERR_NULL_PTR;
    if (!params.alloc) return SCL_ERR_INVALID_ARG;
    scl_allocator_t *alloc = params.alloc;

    scl_ml_linear_t *m = (scl_ml_linear_t *)scl_calloc(
        alloc, 1, sizeof(scl_ml_linear_t), alignof(max_align_t));
    if (scl_unlikely(!m)) return SCL_ERR_OUT_OF_MEMORY;

    m->params = params;
    m->alloc  = alloc;
    m->scratch = scl_alloc_arena_create(alloc, 8192, 0);
    if (!m->scratch) { scl_free(alloc, m); return SCL_ERR_OUT_OF_MEMORY; }
    /* Auto-solver selection */
    if (params.solver == SCL_ML_SOLVER_AUTO) {
        /* Use Normal Equations for small d, SGD for large d */
        if (params.alpha > 0.0 && params.l1_ratio > 0.0 && params.l1_ratio > 0.5)
            m->params.solver = SCL_ML_SOLVER_CD;  /* Lasso-dominated */
        else
            m->params.solver = SCL_ML_SOLVER_SGD;  /* default */
    }

    *model = m;
    return SCL_OK;
}

void
scl_ml_linear_free(scl_ml_linear_t *model) {
    if (scl_unlikely(!model)) return;
    scl_allocator_t *a = model->alloc;
    scl_free(a, model->weights);
    scl_free(a, model->gradient);
    scl_free(a, model->pred_buffer);
    scl_free(a, model->resid_buffer);
    if (model->scratch) scl_alloc_arena_destroy(model->scratch);
    memset(model, 0, sizeof(*model));
    scl_free(a, model);
}

SCL_WARN_UNUSED scl_error_t
scl_ml_linear_fit(scl_ml_linear_t *model, const scl_ml_dataset_t *ds) {
    if (scl_unlikely(!model || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!ds->data || ds->n_rows == 0)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(scl_ml_dataset_has_missing(ds))) return SCL_ERR_ML_MISSING_DATA;

    scl_ml_simd_init();

    scl_alloc_arena_reset(model->scratch);

    size_t d = ds->n_cols;
    model->n_features = d;

    /* Allocate/aligned weights */
    scl_allocator_t *la = model->alloc;
    scl_free(la, model->weights);
    model->weights = (SCL_ML_FLOAT *)scl_calloc(la, d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (scl_unlikely(!model->weights)) return SCL_ERR_OUT_OF_MEMORY;

    /* Allocate working buffers */
    size_t bs = model->params.batch_size > 0 ? model->params.batch_size : ds->n_rows;
    scl_free(la, model->gradient);
    scl_free(la, model->pred_buffer);
    scl_free(la, model->resid_buffer);
    model->gradient     = (SCL_ML_FLOAT *)scl_calloc(la, d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    model->pred_buffer  = (SCL_ML_FLOAT *)scl_calloc(la, bs, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    model->resid_buffer = (SCL_ML_FLOAT *)scl_calloc(la, bs, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (!model->gradient || !model->pred_buffer || !model->resid_buffer)
        return SCL_ERR_OUT_OF_MEMORY;

    model->intercept = 0.0f;
    model->fitted = 0;

    scl_error_t err;
    switch (model->params.solver) {
    case SCL_ML_SOLVER_NORMAL_EQ:
        err = scl_ml_linear_fit_normal(model, ds);
        break;
    case SCL_ML_SOLVER_CD:
        err = scl_ml_linear_fit_cd(model, ds);
        break;
    case SCL_ML_SOLVER_SGD:
    default:
        err = scl_ml_linear_fit_sgd(model, ds);
        break;
    }

    if (err == SCL_OK) model->fitted = 1;
    return err;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_linear_predict(scl_ml_linear_t *model, const scl_ml_dataset_t *ds,
                       SCL_ML_FLOAT *y_out) {
    if (scl_unlikely(!model || !ds || !y_out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(ds->n_cols != model->n_features)) return SCL_ERR_INVALID_ARG;

    size_t n = ds->n_rows;
    size_t d = model->n_features;

    if (n == 0) return SCL_OK;

    for (size_t i = 0; i < n; i++) {
        /* SIMD dot with f64 accumulator — keeps regression precision, vectorizes */
        double pred = (double)scl_ml_simd.dot_f(
            &ds->data[i * ds->row_stride], model->weights, d);
        y_out[i] = (float)(pred + (double)model->intercept);
    }

    return SCL_OK;
}

SCL_WARN_UNUSED SCL_ML_FLOAT
scl_ml_linear_get_intercept(const scl_ml_linear_t *model) {
    return model ? model->intercept : 0.0f;
}

SCL_WARN_UNUSED const SCL_ML_FLOAT *
scl_ml_linear_get_weights(const scl_ml_linear_t *model) {
    return model ? model->weights : NULL;
}

SCL_PURE size_t
scl_ml_linear_get_n_features(const scl_ml_linear_t *model) {
    return model ? model->n_features : 0;
}

/* ── Serialization ───────────────────────────────────────────── */

SCL_WARN_UNUSED scl_error_t
scl_ml_linear_save(const scl_ml_linear_t *model,
                    uint8_t **buf, size_t *len, scl_allocator_t *alloc) {
    if (scl_unlikely(!model || !buf || !len)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!alloc)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;

    size_t weights_bytes = model->n_features * sizeof(SCL_ML_FLOAT);
    scl_ml_serial_header_t hdr;
    hdr.magic     = SCL_ML_MAGIC;
    hdr.version   = SCL_ML_FORMAT_VERSION;
    hdr.algo_id   = SCL_ML_ALGO_LINEAR;
    hdr.header_sz = sizeof(hdr);
    hdr.reserved  = 0;

    /* Payload: [n_features:8][intercept:4][weights_bytes...] */
    size_t payload_sz = sizeof(size_t) + sizeof(SCL_ML_FLOAT) + weights_bytes;
    size_t total = sizeof(hdr) + payload_sz + sizeof(uint32_t); /* crc */

    uint8_t *buffer = (uint8_t *)scl_calloc(alloc, 1, total, alignof(max_align_t));
    if (!buffer) return SCL_ERR_OUT_OF_MEMORY;

    memcpy(buffer, &hdr, sizeof(hdr));
    size_t off = sizeof(hdr);

    memcpy(buffer + off, &model->n_features, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->intercept, sizeof(SCL_ML_FLOAT)); off += sizeof(SCL_ML_FLOAT);
    memcpy(buffer + off, model->weights, weights_bytes); off += weights_bytes;

    uint32_t crc = scl_ml_crc32c(buffer + sizeof(scl_ml_serial_header_t), payload_sz);
    memcpy(buffer + off, &crc, sizeof(crc));

    *buf = buffer;
    *len = total;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_linear_load(scl_ml_linear_t **model,
                    const uint8_t *buf, size_t len,
                    scl_ml_linear_params_t params) {
    if (scl_unlikely(!model || !buf)) return SCL_ERR_NULL_PTR;

    if (scl_unlikely(len < sizeof(scl_ml_serial_header_t) + sizeof(uint32_t)))
        return SCL_ERR_INVALID_ARG;

    const scl_ml_serial_header_t *hdr = (const scl_ml_serial_header_t *)buf;
    if (scl_unlikely(hdr->magic != SCL_ML_MAGIC || hdr->algo_id != SCL_ML_ALGO_LINEAR))
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
    size_t n_features = 0;
    memcpy(&n_features, buf + off, sizeof(size_t)); off += sizeof(size_t);

    scl_ml_linear_t *m;
    scl_ml_linear_params_t p = params;
    p.solver = SCL_ML_SOLVER_SGD; /* don't re-train, use loaded weights */
    scl_error_t err = scl_ml_linear_new(&m, p);
    if (err != SCL_OK) return err;

    m->n_features = n_features;
    m->weights = (SCL_ML_FLOAT *)scl_calloc(m->alloc, n_features, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (!m->weights) { scl_ml_linear_free(m); return SCL_ERR_OUT_OF_MEMORY; }

    memcpy(&m->intercept, buf + off, sizeof(SCL_ML_FLOAT)); off += sizeof(SCL_ML_FLOAT);
    memcpy(m->weights, buf + off, n_features * sizeof(SCL_ML_FLOAT));

    m->fitted = 1;
    *model = m;
    return SCL_OK;
}
