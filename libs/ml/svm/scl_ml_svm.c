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

#include "scl_ml_svm.h"
#include "scl_ml_simd.h"
#include "scl_alloc_arena.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

typedef struct scl_ml_svm {
    scl_ml_svm_params_t params;
    SCL_ML_FLOAT       *support_vectors;
    SCL_ML_FLOAT       *alpha;
    SCL_ML_FLOAT       *sv_labels;
    SCL_ML_FLOAT        b;
    size_t              n_sv;
    size_t              n_features;
    int                 fitted;
    scl_allocator_t    *alloc;
    scl_allocator_t    *scratch;
} scl_ml_svm_t;

static SCL_ML_FLOAT
scl_ml_svm_kernel_linear(const SCL_ML_FLOAT *x, const SCL_ML_FLOAT *y,
                          size_t d) {
    /* SIMD dot with f64 accumulator (numerical stability for large d) */
    return scl_ml_simd.dot_f(x, y, d);
}

static SCL_ML_FLOAT
scl_ml_svm_kernel_rbf(const SCL_ML_FLOAT *x, const SCL_ML_FLOAT *y,
                       size_t d, SCL_ML_FLOAT gamma) {
    /* SIMD squared-distance; single expf (not per-element) keeps accuracy */
    float dist_sq = scl_ml_simd.dist_l2_sq(x, y, d);
    return (SCL_ML_FLOAT)expf(-(float)gamma * dist_sq);
}

static SCL_ML_FLOAT
scl_ml_svm_kernel_poly(const SCL_ML_FLOAT *x, const SCL_ML_FLOAT *y,
                        size_t d, SCL_ML_FLOAT gamma, SCL_ML_FLOAT coef0,
                        int degree) {
    float dot = scl_ml_simd.dot_f(x, y, d);
    float val = (float)gamma * dot + (float)coef0;
    float result = 1.0f;
    int deg = degree;
    while (deg > 0) {
        if (deg & 1) result *= val;
        val *= val;
        deg >>= 1;
    }
    return (SCL_ML_FLOAT)result;
}

static SCL_ML_FLOAT
scl_ml_svm_kernel(const scl_ml_svm_t *model,
                   const SCL_ML_FLOAT *x, const SCL_ML_FLOAT *y) {
    switch (model->params.kernel) {
    case SCL_ML_KERNEL_LINEAR:
        return scl_ml_svm_kernel_linear(x, y, model->n_features);
    case SCL_ML_KERNEL_RBF: {
        SCL_ML_FLOAT gamma = (SCL_ML_FLOAT)model->params.gamma;
        return scl_ml_svm_kernel_rbf(x, y, model->n_features, gamma);
    }
    case SCL_ML_KERNEL_POLY: {
        SCL_ML_FLOAT gamma = (SCL_ML_FLOAT)model->params.gamma;
        SCL_ML_FLOAT coef0 = (SCL_ML_FLOAT)model->params.coef0;
        return scl_ml_svm_kernel_poly(x, y, model->n_features,
                                       gamma, coef0, model->params.degree);
    }
    default:
        return scl_ml_svm_kernel_rbf(x, y, model->n_features,
                                      (SCL_ML_FLOAT)model->params.gamma);
    }
}

static SCL_ML_FLOAT
scl_ml_svm_decision_function(const scl_ml_svm_t *model,
                              const SCL_ML_FLOAT *x) {
    double sum = 0.0;
    for (size_t i = 0; i < model->n_sv; i++) {
        SCL_ML_FLOAT k = scl_ml_svm_kernel(
            model, &model->support_vectors[i * model->n_features], x);
        sum += (double)model->alpha[i] * (double)model->sv_labels[i] * (double)k;
    }
    return (SCL_ML_FLOAT)(sum + (double)model->b);
}

static int
scl_ml_svm_take_step(scl_ml_svm_t *model,
                      const scl_ml_dataset_t *ds,
                      SCL_ML_FLOAT *alphas, SCL_ML_FLOAT *b,
                       SCL_ML_FLOAT *E, SCL_ML_FLOAT tol,
                       SCL_ML_FLOAT C, int i, int j) {
    (void)tol;
    if (i == j) return 0;

    size_t n = ds->n_rows;

    SCL_ML_FLOAT yi = ds->targets[i];
    SCL_ML_FLOAT yj = ds->targets[j];

    SCL_ML_FLOAT Ei = E[i];
    SCL_ML_FLOAT Ej = E[j];

    SCL_ML_FLOAT s = yi * yj;

    SCL_ML_FLOAT L, H;
    if (yi != yj) {
        L = (SCL_ML_FLOAT)fmaxf(0.0f, alphas[j] - alphas[i]);
        H = (SCL_ML_FLOAT)fminf(C, C + alphas[j] - alphas[i]);
    } else {
        L = (SCL_ML_FLOAT)fmaxf(0.0f, alphas[i] + alphas[j] - C);
        H = (SCL_ML_FLOAT)fminf(C, alphas[i] + alphas[j]);
    }

    if (L >= H - 1e-10f) return 0;

    const SCL_ML_FLOAT *xi = &ds->data[i * ds->row_stride];
    const SCL_ML_FLOAT *xj = &ds->data[j * ds->row_stride];

    SCL_ML_FLOAT kii = scl_ml_svm_kernel(model, xi, xi);
    SCL_ML_FLOAT kjj = scl_ml_svm_kernel(model, xj, xj);
    SCL_ML_FLOAT kij = scl_ml_svm_kernel(model, xi, xj);

    SCL_ML_FLOAT eta = kii + kjj - 2.0f * kij;

    if (eta <= 0.0f) {
        SCL_ML_FLOAT alpha_j_old = alphas[j];
        SCL_ML_FLOAT alpha_i_old = alphas[i];

        SCL_ML_FLOAT rest_i = Ei + yi - alpha_i_old * yi * kii - alpha_j_old * yj * kij;
        SCL_ML_FLOAT rest_j = Ej + yj - alpha_i_old * yi * kij - alpha_j_old * yj * kjj;

        SCL_ML_FLOAT delta_aj_L = L - alpha_j_old;
        SCL_ML_FLOAT ai_L = alpha_i_old - s * delta_aj_L;
        SCL_ML_FLOAT obj_L = ai_L + L
            - 0.5f * kii * ai_L * ai_L
            - 0.5f * kjj * L * L
            - s * kij * ai_L * L
            - ai_L * yi * rest_i
            - L * yj * rest_j;

        SCL_ML_FLOAT delta_aj_H = H - alpha_j_old;
        SCL_ML_FLOAT ai_H = alpha_i_old - s * delta_aj_H;
        SCL_ML_FLOAT obj_H = ai_H + H
            - 0.5f * kii * ai_H * ai_H
            - 0.5f * kjj * H * H
            - s * kij * ai_H * H
            - ai_H * yi * rest_i
            - H * yj * rest_j;

        SCL_ML_FLOAT new_ai, new_aj;
        if (obj_L > obj_H + 1e-10f) {
            new_aj = L; new_ai = ai_L;
        } else if (obj_H > obj_L + 1e-10f) {
            new_aj = H; new_ai = ai_H;
        } else {
            return 0;
        }

        SCL_ML_FLOAT b_old = *b;
        SCL_ML_FLOAT delta_ai = new_ai - alpha_i_old;
        SCL_ML_FLOAT delta_aj = new_aj - alpha_j_old;

        SCL_ML_FLOAT b1 = 0.0f, b2 = 0.0f;
        if (new_ai > 0.0f && new_ai < C)
            b1 = b_old - Ei - yi * delta_ai * kii - yj * delta_aj * kij;
        if (new_aj > 0.0f && new_aj < C)
            b2 = b_old - Ej - yi * delta_ai * kij - yj * delta_aj * kjj;

        SCL_ML_FLOAT b_new = (b1 + b2) / 2.0f;
        SCL_ML_FLOAT delta_b = b_new - b_old;
        *b = b_new;
        alphas[i] = new_ai;
        alphas[j] = new_aj;

        for (size_t k = 0; k < n; k++) {
            const SCL_ML_FLOAT *xk = &ds->data[k * ds->row_stride];
            SCL_ML_FLOAT kik = scl_ml_svm_kernel(model, xi, xk);
            SCL_ML_FLOAT kjk = scl_ml_svm_kernel(model, xj, xk);
            E[k] += yi * delta_ai * kik + yj * delta_aj * kjk + delta_b;
        }

        return 1;
    }

    SCL_ML_FLOAT alpha_j_old = alphas[j];
    SCL_ML_FLOAT alpha_i_old = alphas[i];

    alphas[j] = alpha_j_old + yj * (Ei - Ej) / eta;

    if (alphas[j] > H) alphas[j] = H;
    if (alphas[j] < L) alphas[j] = L;

    if (fabsf(alphas[j] - alpha_j_old) < 1e-10f)
        return 0;

    alphas[i] = alpha_i_old - s * (alphas[j] - alpha_j_old);

    if (alphas[i] > C) alphas[i] = C;
    if (alphas[i] < 0.0f) alphas[i] = 0.0f;

    SCL_ML_FLOAT b_old = *b;
    SCL_ML_FLOAT b1, b2;
    SCL_ML_FLOAT delta_ai = alphas[i] - alpha_i_old;
    SCL_ML_FLOAT delta_aj = alphas[j] - alpha_j_old;

    if (alphas[i] > 0.0f && alphas[i] < C)
        b1 = b_old - Ei - yi * delta_ai * kii - yj * delta_aj * kij;
    else
        b1 = 0.0f;

    if (alphas[j] > 0.0f && alphas[j] < C)
        b2 = b_old - Ej - yi * delta_ai * kij - yj * delta_aj * kjj;
    else
        b2 = 0.0f;

    SCL_ML_FLOAT b_new = (b1 + b2) / 2.0f;
    SCL_ML_FLOAT delta_b = b_new - b_old;
    *b = b_new;

    for (size_t k = 0; k < n; k++) {
        const SCL_ML_FLOAT *xk = &ds->data[k * ds->row_stride];
        SCL_ML_FLOAT kik = scl_ml_svm_kernel(model, xi, xk);
        SCL_ML_FLOAT kjk = scl_ml_svm_kernel(model, xj, xk);
        E[k] += yi * delta_ai * kik + yj * delta_aj * kjk + delta_b;
    }

    return 1;
}

static int
scl_ml_svm_examine_example(scl_ml_svm_t *model,
                            const scl_ml_dataset_t *ds,
                            SCL_ML_FLOAT *alphas, SCL_ML_FLOAT *b,
                            SCL_ML_FLOAT *E, SCL_ML_FLOAT tol,
                            SCL_ML_FLOAT C, int i) {
    size_t n = ds->n_rows;

    SCL_ML_FLOAT yi = ds->targets[i];
    SCL_ML_FLOAT Ei = E[i];
    SCL_ML_FLOAT r1 = yi * Ei;

    if ((r1 < -tol && alphas[i] < C) || (r1 > tol && alphas[i] > 0.0f)) {
        int max_j = -1;
        SCL_ML_FLOAT max_diff = 0.0f;

        for (size_t j = 0; j < n; j++) {
            if (j == (size_t)i) continue;
            SCL_ML_FLOAT diff = fabsf(E[i] - E[j]);
            if (diff > max_diff) {
                max_diff = diff;
                max_j = (int)j;
            }
        }

        if (max_j >= 0) {
            if (scl_ml_svm_take_step(model, ds, alphas, b, E, tol, C,
                                      i, max_j))
                return 1;
        }

        for (size_t j = 0; j < n; j++) {
            if (j == (size_t)i) continue;
            if (scl_ml_svm_take_step(model, ds, alphas, b, E, tol, C,
                                      i, (int)j))
                return 1;
        }
    }

    return 0;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_svm_new(scl_ml_svm_t **model, scl_ml_svm_params_t params) {
    if (scl_unlikely(!model)) return SCL_ERR_NULL_PTR;
    if (!params.alloc) return SCL_ERR_INVALID_ARG;
    scl_allocator_t *alloc = params.alloc;
    scl_ml_svm_t *m = (scl_ml_svm_t *)scl_calloc(alloc, 1, sizeof(scl_ml_svm_t), alignof(max_align_t));
    if (scl_unlikely(!m)) return SCL_ERR_OUT_OF_MEMORY;
    m->params = params;
    m->alloc = alloc;
    m->scratch = scl_alloc_arena_create(alloc, 8192, 0);
    if (!m->scratch) { scl_free(alloc, m); return SCL_ERR_OUT_OF_MEMORY; }
    *model = m;
    return SCL_OK;
}

void
scl_ml_svm_free(scl_ml_svm_t *model) {
    if (scl_unlikely(!model)) return;
    scl_allocator_t *a = model->alloc;
    scl_free(a, model->support_vectors);
    scl_free(a, model->alpha);
    scl_free(a, model->sv_labels);
    if (model->scratch) scl_alloc_arena_destroy(model->scratch);
    memset(model, 0, sizeof(*model));
    scl_free(a, model);
}

SCL_WARN_UNUSED scl_error_t
scl_ml_svm_fit(scl_ml_svm_t *model, const scl_ml_dataset_t *ds) {
    if (scl_unlikely(!model || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!ds->data || ds->n_rows == 0)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(scl_ml_dataset_has_missing(ds))) return SCL_ERR_ML_MISSING_DATA;

    scl_ml_simd_init();

    size_t n = ds->n_rows;
    size_t d = ds->n_cols;
    model->n_features = d;
    model->fitted = 0;

    if (model->params.gamma <= 0.0)
        model->params.gamma = 1.0 / (double)d;

    scl_alloc_arena_reset(model->scratch);

    SCL_ML_FLOAT C = (SCL_ML_FLOAT)model->params.C;
    SCL_ML_FLOAT tol = (SCL_ML_FLOAT)model->params.tol;
    int max_iter = model->params.max_iter;
    if (max_iter < 0) max_iter = (int)(n * n * 10);
    if (max_iter < 100) max_iter = 100;

    scl_allocator_t *a = model->alloc;
    SCL_ML_FLOAT *alphas = (SCL_ML_FLOAT *)scl_calloc(model->scratch, n, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    SCL_ML_FLOAT *E = (SCL_ML_FLOAT *)scl_calloc(model->scratch, n, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (!alphas || !E) {
        return SCL_ERR_OUT_OF_MEMORY;
    }

    SCL_ML_FLOAT b = 0.0f;

    for (size_t i = 0; i < n; i++)
        E[i] = -ds->targets[i];

    int num_changed = 0;
    int examine_all = 1;
    int iteration = 0;

    while (iteration < max_iter) {
        num_changed = 0;

        if (examine_all) {
            for (size_t i = 0; i < n; i++) {
                if (scl_ml_svm_examine_example(
                        model, ds, alphas, &b, E, tol, C, (int)i))
                    num_changed++;
            }
        } else {
            for (size_t i = 0; i < n; i++) {
                if (alphas[i] > 0.0f && alphas[i] < C) {
                    if (scl_ml_svm_examine_example(
                            model, ds, alphas, &b, E, tol, C, (int)i))
                        num_changed++;
                }
            }
        }

        if (num_changed == 0) {
            if (examine_all) break;
            examine_all = 1;
        } else if (examine_all) {
            examine_all = 0;
        }

        iteration++;
    }

    size_t sv_count = 0;
    for (size_t i = 0; i < n; i++) {
        if (alphas[i] > 1e-6f)
            sv_count++;
    }

    model->n_sv = sv_count;
    model->b = b;

    if (sv_count > 0) {
        model->support_vectors = (SCL_ML_FLOAT *)scl_calloc(
            a, sv_count * d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
        model->alpha = (SCL_ML_FLOAT *)scl_calloc(
            a, sv_count, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
        model->sv_labels = (SCL_ML_FLOAT *)scl_calloc(
            a, sv_count, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
        if (!model->support_vectors || !model->alpha || !model->sv_labels) {
            scl_free(a, model->support_vectors);
            scl_free(a, model->alpha);
            scl_free(a, model->sv_labels);
            return SCL_ERR_OUT_OF_MEMORY;
        }

        size_t idx = 0;
        for (size_t i = 0; i < n; i++) {
            if (alphas[i] > 1e-6f) {
                memcpy(&model->support_vectors[idx * d],
                       &ds->data[i * ds->row_stride],
                       d * sizeof(SCL_ML_FLOAT));
                model->alpha[idx] = alphas[i];
                model->sv_labels[idx] = ds->targets[i];
                idx++;
            }
        }
    }

    model->fitted = 1;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_svm_predict(scl_ml_svm_t *model, const scl_ml_dataset_t *ds,
                    SCL_ML_FLOAT *y_out) {
    if (scl_unlikely(!model || !ds || !y_out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(ds->n_cols != model->n_features)) return SCL_ERR_INVALID_ARG;

    for (size_t i = 0; i < ds->n_rows; i++) {
        SCL_ML_FLOAT fx = scl_ml_svm_decision_function(
            model, &ds->data[i * ds->row_stride]);
        y_out[i] = (fx >= 0.0f) ? 1.0f : -1.0f;
    }

    return SCL_OK;
}

SCL_PURE size_t
scl_ml_svm_get_n_features(const scl_ml_svm_t *model) {
    return model ? model->n_features : 0;
}

SCL_PURE size_t
scl_ml_svm_get_n_support(const scl_ml_svm_t *model) {
    return model ? model->n_sv : 0;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_svm_save(const scl_ml_svm_t *model,
                 uint8_t **buf, size_t *len, scl_allocator_t *alloc) {
    if (scl_unlikely(!model || !buf || !len)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(!alloc)) return SCL_ERR_NULL_PTR;

    scl_ml_serial_header_t hdr;
    hdr.magic     = SCL_ML_MAGIC;
    hdr.version   = SCL_ML_FORMAT_VERSION;
    hdr.algo_id   = SCL_ML_ALGO_SVM;
    hdr.header_sz = sizeof(hdr);
    hdr.reserved  = 0;
    hdr.crc32c    = 0;

    size_t sv_bytes = model->n_sv * model->n_features * sizeof(SCL_ML_FLOAT);
    size_t alpha_bytes = model->n_sv * sizeof(SCL_ML_FLOAT);
    size_t labels_bytes = model->n_sv * sizeof(SCL_ML_FLOAT);

    size_t payload_sz = sizeof(size_t) * 2 + sizeof(SCL_ML_FLOAT) +
                        sv_bytes + alpha_bytes + labels_bytes;
    size_t total = sizeof(hdr) + payload_sz + sizeof(uint32_t);

    uint8_t *buffer = (uint8_t *)scl_calloc(alloc, 1, total, alignof(max_align_t));
    if (!buffer) return SCL_ERR_OUT_OF_MEMORY;

    memcpy(buffer, &hdr, sizeof(hdr));
    size_t off = sizeof(hdr);

    memcpy(buffer + off, &model->n_sv, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->n_features, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &model->b, sizeof(SCL_ML_FLOAT)); off += sizeof(SCL_ML_FLOAT);
    memcpy(buffer + off, model->support_vectors, sv_bytes); off += sv_bytes;
    memcpy(buffer + off, model->alpha, alpha_bytes); off += alpha_bytes;
    memcpy(buffer + off, model->sv_labels, labels_bytes); off += labels_bytes;

    uint32_t crc = scl_ml_crc32c(buffer + sizeof(hdr), payload_sz);
    memcpy(buffer + off, &crc, sizeof(crc));

    *buf = buffer;
    *len = total;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_svm_load(scl_ml_svm_t **model,
                 const uint8_t *buf, size_t len,
                 scl_ml_svm_params_t params) {
    if (scl_unlikely(!model || !buf)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(len < sizeof(scl_ml_serial_header_t) + sizeof(uint32_t)))
        return SCL_ERR_INVALID_ARG;

    const scl_ml_serial_header_t *hdr = (const scl_ml_serial_header_t *)buf;
    if (scl_unlikely(hdr->magic != SCL_ML_MAGIC ||
                     hdr->algo_id != SCL_ML_ALGO_SVM))
        return SCL_ERR_INVALID_ARG;

    scl_ml_svm_t *m;
    scl_error_t err = scl_ml_svm_new(&m, params);
    if (err != SCL_OK) return err;

    size_t off = sizeof(*hdr);
    scl_allocator_t *a = m->alloc;

    memcpy(&m->n_sv, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&m->n_features, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&m->b, buf + off, sizeof(SCL_ML_FLOAT)); off += sizeof(SCL_ML_FLOAT);

    if (m->n_sv > len || m->n_features == 0) {
        scl_ml_svm_free(m);
        return SCL_ERR_INVALID_ARG;
    }

    size_t sv_bytes = m->n_sv * m->n_features * sizeof(SCL_ML_FLOAT);
    size_t alpha_bytes = m->n_sv * sizeof(SCL_ML_FLOAT);
    size_t labels_bytes = m->n_sv * sizeof(SCL_ML_FLOAT);

    if (off + sv_bytes + alpha_bytes + labels_bytes > len) {
        scl_ml_svm_free(m);
        return SCL_ERR_INVALID_ARG;
    }

    m->support_vectors = (SCL_ML_FLOAT *)scl_calloc(
        a, m->n_sv * m->n_features, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    m->alpha = (SCL_ML_FLOAT *)scl_calloc(a, m->n_sv, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    m->sv_labels = (SCL_ML_FLOAT *)scl_calloc(a, m->n_sv, sizeof(SCL_ML_FLOAT), alignof(max_align_t));

    if (!m->support_vectors || !m->alpha || !m->sv_labels) {
        scl_ml_svm_free(m);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    memcpy(m->support_vectors, buf + off, sv_bytes); off += sv_bytes;
    memcpy(m->alpha, buf + off, alpha_bytes); off += alpha_bytes;
    memcpy(m->sv_labels, buf + off, labels_bytes);

    uint32_t stored_crc = 0;
    memcpy(&stored_crc, buf + len - sizeof(uint32_t), sizeof(uint32_t));
    uint32_t expected_crc = scl_ml_crc32c(buf + sizeof(*hdr), len - sizeof(*hdr) - sizeof(uint32_t));
    if (stored_crc != expected_crc) {
        scl_ml_svm_free(m);
        return SCL_ERR_INVALID_ARG;
    }

    m->fitted = 1;
    *model = m;
    return SCL_OK;
}
