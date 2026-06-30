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

#include "scl_ml_nb.h"
#include "scl_ml_simd.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

#define SCL_ML_NB_PI 3.14159265358979323846f

typedef struct scl_ml_nb {
    scl_ml_nb_params_t params;
    SCL_ML_FLOAT *means;
    SCL_ML_FLOAT *vars;
    SCL_ML_FLOAT *class_log_prior;
    int    *class_labels;
    size_t  n_classes;
    size_t  n_features;
    int     fitted;

    SCL_ML_FLOAT *lp_buffer;
    /* Precomputed per-(class,feature) terms to keep the predict hot loop
     * free of logf() and division:
     *   log_term[c*nf+f] = -0.5 * log(2*pi*var)
     *   inv_2var[c*nf+f] = 0.5 / var
     * Built at fit/load from vars[]. */
    SCL_ML_FLOAT *log_term;
    SCL_ML_FLOAT *inv_2var;
    scl_allocator_t *alloc;
} scl_ml_nb_t;

static int
scl_ml_nb_find_class(const int *labels, size_t n_classes, int label) {
    for (size_t c = 0; c < n_classes; c++)
        if (labels[c] == label) return (int)c;
    return -1;
}

/* Rebuild log_term/inv_2var from finalized vars[]. Called at fit and load. */
static void
scl_ml_nb_precompute_gaussian(scl_ml_nb_t *model) {
    size_t nf = model->n_features;
    size_t nc = model->n_classes;
    float two_pi = 2.0f * SCL_ML_NB_PI;
    for (size_t c = 0; c < nc; c++) {
        for (size_t f = 0; f < nf; f++) {
            float v = model->vars[c * nf + f];
            model->log_term[c * nf + f]  = -0.5f * logf(two_pi * v);
            model->inv_2var[c * nf + f]  = 0.5f / v;
        }
    }
}

SCL_WARN_UNUSED scl_error_t
scl_ml_nb_new(scl_ml_nb_t **model, scl_ml_nb_params_t params) {
    if (scl_unlikely(!model)) return SCL_ERR_NULL_PTR;

    scl_allocator_t *alloc = params.alloc ? params.alloc : scl_allocator_default();
    scl_ml_nb_t *m = (scl_ml_nb_t *)scl_calloc(
        alloc, 1, sizeof(scl_ml_nb_t), alignof(max_align_t));
    if (scl_unlikely(!m)) return SCL_ERR_OUT_OF_MEMORY;

    m->params = params;
    m->alloc  = alloc;
    *model = m;
    return SCL_OK;
}

void
scl_ml_nb_free(scl_ml_nb_t *model) {
    if (scl_unlikely(!model)) return;
    scl_allocator_t *a = model->alloc ? model->alloc : scl_allocator_default();
    scl_free(a, model->means);
    scl_free(a, model->vars);
    scl_free(a, model->class_log_prior);
    scl_free(a, model->class_labels);
    scl_free(a, model->lp_buffer);
    scl_free(a, model->log_term);
    scl_free(a, model->inv_2var);
    memset(model, 0, sizeof(*model));
    scl_free(a, model);
}

SCL_WARN_UNUSED scl_error_t
scl_ml_nb_fit(scl_ml_nb_t *model, const scl_ml_dataset_t *ds) {
    if (scl_unlikely(!model || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!ds->data || !ds->targets || ds->n_rows == 0))
        return SCL_ERR_INVALID_ARG;

    scl_ml_simd_init();

    size_t n = ds->n_rows;
    size_t nf = ds->n_cols;
    scl_allocator_t *a = model->alloc;

    int n_classes = 0;
    int *labels = (int *)scl_calloc(a, n, sizeof(int), alignof(max_align_t));
    if (!labels) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t i = 0; i < n; i++) {
        int label = (int)ds->targets[i];
        if (scl_ml_nb_find_class(labels, (size_t)n_classes, label) < 0)
            labels[n_classes++] = label;
    }

    if (n_classes < 2) {
        scl_free(a, labels);
        return SCL_ERR_INVALID_ARG;
    }

    int *counts = (int *)scl_calloc(a, (size_t)n_classes, sizeof(int), alignof(max_align_t));
    if (!counts) { scl_free(a, labels); return SCL_ERR_OUT_OF_MEMORY; }

    for (size_t i = 0; i < n; i++) {
        int label = (int)ds->targets[i];
        int c = scl_ml_nb_find_class(labels, (size_t)n_classes, label);
        counts[c]++;
    }

    scl_free(a, model->means);
    scl_free(a, model->vars);
    scl_free(a, model->class_log_prior);
    scl_free(a, model->class_labels);
    scl_free(a, model->lp_buffer);
    scl_free(a, model->log_term);
    scl_free(a, model->inv_2var);

    size_t cv_sz = (size_t)n_classes * nf;
    model->means = (SCL_ML_FLOAT *)scl_calloc(a, cv_sz, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    model->vars = (SCL_ML_FLOAT *)scl_calloc(a, cv_sz, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    model->class_log_prior = (SCL_ML_FLOAT *)scl_calloc(a, (size_t)n_classes, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    model->class_labels = (int *)scl_calloc(a, (size_t)n_classes, sizeof(int), alignof(max_align_t));
    model->lp_buffer = (SCL_ML_FLOAT *)scl_calloc(a, (size_t)n_classes, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    model->log_term  = (SCL_ML_FLOAT *)scl_calloc(a, cv_sz, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    model->inv_2var  = (SCL_ML_FLOAT *)scl_calloc(a, cv_sz, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (!model->means || !model->vars || !model->class_log_prior ||
        !model->class_labels || !model->lp_buffer ||
        !model->log_term || !model->inv_2var) {
        scl_free(a, labels); scl_free(a, counts);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    memcpy(model->class_labels, labels, (size_t)n_classes * sizeof(int));
    model->n_classes = (size_t)n_classes;
    model->n_features = nf;

    for (size_t i = 0; i < n; i++) {
        int c = scl_ml_nb_find_class(labels, (size_t)n_classes, (int)ds->targets[i]);
        for (size_t f = 0; f < nf; f++)
            model->means[c * nf + f] += ds->data[i * ds->row_stride + f];
    }

    for (int c = 0; c < n_classes; c++) {
        float inv_cnt = 1.0f / (float)counts[c];
        for (size_t f = 0; f < nf; f++)
            model->means[c * nf + f] *= inv_cnt;
    }

    for (size_t i = 0; i < n; i++) {
        int c = scl_ml_nb_find_class(labels, (size_t)n_classes, (int)ds->targets[i]);
        for (size_t f = 0; f < nf; f++) {
            float diff = ds->data[i * ds->row_stride + f] - model->means[c * nf + f];
            model->vars[c * nf + f] += diff * diff;
        }
    }

    float smoothing = (float)model->params.var_smoothing;
    for (int c = 0; c < n_classes; c++) {
        float inv_cnt = 1.0f / (float)counts[c];
        for (size_t f = 0; f < nf; f++)
            model->vars[c * nf + f] = model->vars[c * nf + f] * inv_cnt + smoothing;
    }

    float inv_n = 1.0f / (float)n;
    for (int c = 0; c < n_classes; c++)
        model->class_log_prior[c] = logf((float)counts[c] * inv_n);

    scl_ml_nb_precompute_gaussian(model);

    scl_free(a, labels);
    scl_free(a, counts);

    model->fitted = 1;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_nb_predict(scl_ml_nb_t *model, const scl_ml_dataset_t *ds,
                   SCL_ML_FLOAT *y_out) {
    if (scl_unlikely(!model || !ds || !y_out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(ds->n_cols != model->n_features)) return SCL_ERR_INVALID_ARG;

    size_t n = ds->n_rows;
    size_t nf = model->n_features;
    size_t nc = model->n_classes;

    for (size_t i = 0; i < n; i++) {
        for (size_t c = 0; c < nc; c++)
            model->lp_buffer[c] = model->class_log_prior[c];

        for (size_t c = 0; c < nc; c++) {
            const SCL_ML_FLOAT *m = &model->means[c * nf];
            const SCL_ML_FLOAT *lt = &model->log_term[c * nf];
            const SCL_ML_FLOAT *i2v = &model->inv_2var[c * nf];
            float lp = model->lp_buffer[c];
            for (size_t f = 0; f < nf; f++) {
                float diff = ds->data[i * ds->row_stride + f] - m[f];
                lp += lt[f] - diff * diff * i2v[f];
            }
            model->lp_buffer[c] = lp;
        }

        size_t best = scl_ml_simd.argmax(model->lp_buffer, nc);
        y_out[i] = (SCL_ML_FLOAT)model->class_labels[best];
    }

    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_nb_predict_proba(scl_ml_nb_t *model, const scl_ml_dataset_t *ds,
                          SCL_ML_FLOAT *proba_out) {
    if (scl_unlikely(!model || !ds || !proba_out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(ds->n_cols != model->n_features)) return SCL_ERR_INVALID_ARG;

    size_t n = ds->n_rows;
    size_t nf = model->n_features;
    size_t nc = model->n_classes;

    for (size_t i = 0; i < n; i++) {
        for (size_t c = 0; c < nc; c++)
            model->lp_buffer[c] = model->class_log_prior[c];

        for (size_t c = 0; c < nc; c++) {
            const SCL_ML_FLOAT *m = &model->means[c * nf];
            const SCL_ML_FLOAT *lt = &model->log_term[c * nf];
            const SCL_ML_FLOAT *i2v = &model->inv_2var[c * nf];
            float lp = model->lp_buffer[c];
            for (size_t f = 0; f < nf; f++) {
                float diff = ds->data[i * ds->row_stride + f] - m[f];
                lp += lt[f] - diff * diff * i2v[f];
            }
            model->lp_buffer[c] = lp;
        }

        scl_ml_simd.softmax(proba_out + i * nc, model->lp_buffer, nc);
    }

    return SCL_OK;
}

SCL_PURE size_t
scl_ml_nb_get_n_classes(const scl_ml_nb_t *model) {
    return model ? model->n_classes : 0;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_nb_save(const scl_ml_nb_t *model,
                uint8_t **buf, size_t *len, scl_allocator_t *alloc) {
    if (scl_unlikely(!model || !buf || !len)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(!alloc)) alloc = model->alloc ? model->alloc : scl_allocator_default();

    scl_ml_serial_header_t hdr;
    hdr.magic     = SCL_ML_MAGIC;
    hdr.version   = SCL_ML_FORMAT_VERSION;
    hdr.algo_id   = SCL_ML_ALGO_NAIVE_BAYES;
    hdr.header_sz = sizeof(hdr);
    hdr.reserved  = 0;

    size_t nc = model->n_classes;
    size_t nf = model->n_features;
    size_t cv_sz = nc * nf;

    size_t payload_sz = sizeof(size_t) * 2 +
                        cv_sz * sizeof(SCL_ML_FLOAT) * 2 +
                        nc * sizeof(SCL_ML_FLOAT);
    size_t total = sizeof(hdr) + payload_sz + sizeof(uint32_t);

    uint8_t *buffer = (uint8_t *)scl_calloc(alloc, 1, total, alignof(max_align_t));
    if (!buffer) return SCL_ERR_OUT_OF_MEMORY;

    memcpy(buffer, &hdr, sizeof(hdr));
    size_t off = sizeof(hdr);

    memcpy(buffer + off, &nc, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &nf, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, model->means, cv_sz * sizeof(SCL_ML_FLOAT)); off += cv_sz * sizeof(SCL_ML_FLOAT);
    memcpy(buffer + off, model->vars, cv_sz * sizeof(SCL_ML_FLOAT)); off += cv_sz * sizeof(SCL_ML_FLOAT);
    memcpy(buffer + off, model->class_log_prior, nc * sizeof(SCL_ML_FLOAT));

    uint32_t crc = scl_ml_crc32c(buffer + sizeof(scl_ml_serial_header_t), payload_sz);
    memcpy(buffer + total - sizeof(uint32_t), &crc, sizeof(crc));

    *buf = buffer;
    *len = total;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_nb_load(scl_ml_nb_t **model,
                const uint8_t *buf, size_t len,
                scl_ml_nb_params_t params) {
    if (scl_unlikely(!model || !buf)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(len < sizeof(scl_ml_serial_header_t) + sizeof(uint32_t)))
        return SCL_ERR_INVALID_ARG;

    const scl_ml_serial_header_t *hdr = (const scl_ml_serial_header_t *)buf;
    if (scl_unlikely(hdr->magic != SCL_ML_MAGIC || hdr->algo_id != SCL_ML_ALGO_NAIVE_BAYES))
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
    size_t nc = 0, nf = 0;
    memcpy(&nc, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&nf, buf + off, sizeof(size_t)); off += sizeof(size_t);

    if (nc == 0 || nf == 0) return SCL_ERR_INVALID_ARG;

    scl_ml_nb_t *m;
    scl_error_t err = scl_ml_nb_new(&m, params);
    if (err != SCL_OK) return err;

    size_t cv_sz = nc * nf;
    m->n_classes = nc;
    m->n_features = nf;

    scl_allocator_t *a = m->alloc;
    m->means = (SCL_ML_FLOAT *)scl_calloc(a, cv_sz, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    m->vars = (SCL_ML_FLOAT *)scl_calloc(a, cv_sz, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    m->class_log_prior = (SCL_ML_FLOAT *)scl_calloc(a, nc, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    m->class_labels = (int *)scl_calloc(a, nc, sizeof(int), alignof(max_align_t));
    m->lp_buffer = (SCL_ML_FLOAT *)scl_calloc(a, nc, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    m->log_term  = (SCL_ML_FLOAT *)scl_calloc(a, cv_sz, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    m->inv_2var  = (SCL_ML_FLOAT *)scl_calloc(a, cv_sz, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (!m->means || !m->vars || !m->class_log_prior ||
        !m->class_labels || !m->lp_buffer ||
        !m->log_term || !m->inv_2var) {
        scl_ml_nb_free(m);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    for (size_t c = 0; c < nc; c++)
        m->class_labels[c] = (int)c;

    memcpy(m->means, buf + off, cv_sz * sizeof(SCL_ML_FLOAT)); off += cv_sz * sizeof(SCL_ML_FLOAT);
    memcpy(m->vars, buf + off, cv_sz * sizeof(SCL_ML_FLOAT)); off += cv_sz * sizeof(SCL_ML_FLOAT);
    memcpy(m->class_log_prior, buf + off, nc * sizeof(SCL_ML_FLOAT));

    scl_ml_nb_precompute_gaussian(m);

    m->fitted = 1;
    *model = m;
    return SCL_OK;
}
