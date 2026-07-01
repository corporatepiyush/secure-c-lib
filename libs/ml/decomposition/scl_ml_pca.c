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

#include "scl_ml_pca.h"
#include "scl_ml_simd.h"
#include "scl_alloc_arena.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

typedef struct scl_ml_pca {
    scl_ml_pca_params_t params;
    SCL_ML_FLOAT *components;
    SCL_ML_FLOAT *mean;
    SCL_ML_FLOAT *explained_variance;
    SCL_ML_FLOAT *explained_variance_ratio;
    size_t        n_components;
    size_t        n_features;
    int           fitted;
    scl_allocator_t *alloc;
    scl_allocator_t *scratch;
} scl_ml_pca_t;

typedef struct {
    size_t idx;
    float  val;
} scl_pca_eigen_t;

static int scl_pca_eigen_desc(const void *a, const void *b) {
    float va = ((const scl_pca_eigen_t *)a)->val;
    float vb = ((const scl_pca_eigen_t *)b)->val;
    if (va > vb) return -1;
    if (va < vb) return  1;
    return 0;
}

static scl_error_t scl_ml_pca_jacobi(SCL_ML_FLOAT *A, SCL_ML_FLOAT *V,
                                      size_t d, double tol, size_t max_sweeps) {
    for (size_t i = 0; i < d; i++) {
        memset(&V[i * d], 0, d * sizeof(SCL_ML_FLOAT));
        V[i * d + i] = 1.0f;
    }

    for (size_t sweep = 0; sweep < max_sweeps; sweep++) {
        double max_off = 0.0;
        size_t p = 0, q = 0;
        for (size_t i = 0; i < d; i++) {
            for (size_t j = i + 1; j < d; j++) {
                double val = fabs((double)A[i * d + j]);
                if (val > max_off) {
                    max_off = val;
                    p = i;
                    q = j;
                }
            }
        }

        if (max_off < tol)
            return SCL_OK;

        float theta = 0.5f * atan2f(2.0f * A[p * d + q],
                                     A[q * d + q] - A[p * d + p]);
        float c = cosf(theta);
        float s = sinf(theta);

        for (size_t i = 0; i < d; i++) {
            if (i != p && i != q) {
                float a_ip = A[i * d + p];
                float a_iq = A[i * d + q];
                A[i * d + p] = c * a_ip - s * a_iq;
                A[i * d + q] = s * a_ip + c * a_iq;
                A[p * d + i] = A[i * d + p];
                A[q * d + i] = A[i * d + q];
            }
        }

        float a_pp = A[p * d + p];
        float a_qq = A[q * d + q];
        float a_pq = A[p * d + q];
        A[p * d + p] = c * c * a_pp + s * s * a_qq - 2.0f * c * s * a_pq;
        A[q * d + q] = s * s * a_pp + c * c * a_qq + 2.0f * c * s * a_pq;
        A[p * d + q] = 0.0f;
        A[q * d + p] = 0.0f;

        for (size_t i = 0; i < d; i++) {
            float v_ip = V[i * d + p];
            float v_iq = V[i * d + q];
            V[i * d + p] = c * v_ip - s * v_iq;
            V[i * d + q] = s * v_ip + c * v_iq;
        }
    }

    return SCL_ERR_ML_CONVERGENCE;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_pca_new(scl_ml_pca_t **model, scl_ml_pca_params_t params) {
    if (scl_unlikely(!model)) return SCL_ERR_NULL_PTR;
    if (!params.alloc) return SCL_ERR_INVALID_ARG;
    scl_allocator_t *alloc = params.alloc;
    scl_ml_pca_t *m = (scl_ml_pca_t *)scl_calloc(
        alloc, 1, sizeof(scl_ml_pca_t), alignof(max_align_t));
    if (scl_unlikely(!m)) return SCL_ERR_OUT_OF_MEMORY;

    m->params = params;
    m->alloc  = alloc;
    m->scratch = scl_alloc_arena_create(alloc, 8192, 0);
    if (!m->scratch) { scl_free(alloc, m); return SCL_ERR_OUT_OF_MEMORY; }
    *model = m;
    return SCL_OK;
}

void
scl_ml_pca_free(scl_ml_pca_t *model) {
    if (scl_unlikely(!model)) return;
    scl_allocator_t *a = model->alloc;
    scl_free(a, model->components);
    scl_free(a, model->mean);
    scl_free(a, model->explained_variance);
    scl_free(a, model->explained_variance_ratio);
    if (model->scratch) scl_alloc_arena_destroy(model->scratch);
    memset(model, 0, sizeof(*model));
    scl_free(a, model);
}

SCL_WARN_UNUSED scl_error_t
scl_ml_pca_fit(scl_ml_pca_t *model, const scl_ml_dataset_t *ds) {
    if (scl_unlikely(!model || !ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!ds->data || ds->n_rows < 2)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(scl_ml_dataset_has_missing(ds))) return SCL_ERR_ML_MISSING_DATA;

    scl_ml_simd_init();

    size_t n = ds->n_rows;
    size_t d = ds->n_cols;
    size_t stride = ds->row_stride;
    size_t n_comp = model->params.n_components;
    if (n_comp == 0 || n_comp > d) n_comp = d;
    if (n_comp > n) n_comp = n;

    model->n_features = d;
    model->n_components = n_comp;

    scl_alloc_arena_reset(model->scratch);
    scl_allocator_t *a = model->alloc;

    scl_free(a, model->mean);
    scl_free(a, model->components);
    scl_free(a, model->explained_variance);
    scl_free(a, model->explained_variance_ratio);

    model->mean = (SCL_ML_FLOAT *)scl_calloc(a, d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    model->components = (SCL_ML_FLOAT *)scl_calloc(a, n_comp * d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    model->explained_variance = (SCL_ML_FLOAT *)scl_calloc(a, n_comp, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    model->explained_variance_ratio = (SCL_ML_FLOAT *)scl_calloc(a, n_comp, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (!model->mean || !model->components ||
        !model->explained_variance || !model->explained_variance_ratio) {
        return SCL_ERR_OUT_OF_MEMORY;
    }

    SCL_ML_FLOAT *cov = (SCL_ML_FLOAT *)scl_calloc(model->scratch, d * d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    SCL_ML_FLOAT *V = (SCL_ML_FLOAT *)scl_calloc(model->scratch, d * d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    SCL_ML_FLOAT *xc = (SCL_ML_FLOAT *)scl_calloc(model->scratch, d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (!cov || !V || !xc) { return SCL_ERR_OUT_OF_MEMORY; }

    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < d; j++)
            model->mean[j] += ds->data[i * stride + j];
    }
    for (size_t j = 0; j < d; j++)
        model->mean[j] /= (SCL_ML_FLOAT)n;

    /* Center each row once into xc[]; update only the upper triangle.
     * Avoids re-subtracting mean[k] in the inner loop (O(n*d) vs O(n*d^2)). */
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < d; j++)
            xc[j] = ds->data[i * stride + j] - model->mean[j];
        for (size_t j = 0; j < d; j++) {
            SCL_ML_FLOAT xcj = xc[j];
            for (size_t k = j; k < d; k++)
                cov[j * d + k] += xcj * xc[k];
        }
    }

    SCL_ML_FLOAT inv_n1 = 1.0f / (SCL_ML_FLOAT)(n - 1);
    for (size_t j = 0; j < d; j++) {
        for (size_t k = j; k < d; k++) {
            cov[j * d + k] *= inv_n1;
            cov[k * d + j] = cov[j * d + k];
        }
    }

    scl_error_t err = scl_ml_pca_jacobi(cov, V, d,
                                         model->params.tol,
                                         model->params.max_sweeps);
    if (scl_unlikely(err != SCL_OK)) {
        return err;
    }

    scl_pca_eigen_t *eigens = (scl_pca_eigen_t *)scl_alloc(model->scratch, d * sizeof(scl_pca_eigen_t), alignof(max_align_t));
    if (!eigens) { return SCL_ERR_OUT_OF_MEMORY; }

    for (size_t j = 0; j < d; j++) {
        eigens[j].idx = j;
        eigens[j].val = cov[j * d + j];
    }
    qsort(eigens, d, sizeof(scl_pca_eigen_t), scl_pca_eigen_desc);

    SCL_ML_FLOAT total_eigen = 0.0f;
    for (size_t j = 0; j < d; j++)
        total_eigen += eigens[j].val;

    for (size_t k = 0; k < n_comp; k++) {
        size_t src = eigens[k].idx;
        model->explained_variance[k] = eigens[k].val;
        model->explained_variance_ratio[k] =
            (total_eigen > 0.0f) ? eigens[k].val / total_eigen : 0.0f;
        for (size_t j = 0; j < d; j++)
            model->components[k * d + j] = V[j * d + src];
    }

    model->fitted = 1;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_pca_transform(scl_ml_pca_t *model, const scl_ml_dataset_t *ds,
                      SCL_ML_FLOAT *out) {
    if (scl_unlikely(!model || !ds || !out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(ds->n_cols != model->n_features)) return SCL_ERR_INVALID_ARG;

    size_t n = ds->n_rows;
    size_t d = model->n_features;
    size_t n_comp = model->n_components;
    size_t stride = ds->row_stride;

    scl_alloc_arena_reset(model->scratch);

    /* Precompute mean·component_k (independent of i) so the hot loop only
     * does one SIMD dot per (sample, component) instead of subtracting mean
     * per element.  (x - mean)·c_k = x·c_k - mean·c_k */
    SCL_ML_FLOAT *mean_dot = (SCL_ML_FLOAT *)scl_calloc(model->scratch, n_comp, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (!mean_dot) return SCL_ERR_OUT_OF_MEMORY;
    for (size_t k = 0; k < n_comp; k++)
        mean_dot[k] = scl_ml_simd.dot_f(model->mean,
                                         &model->components[k * d], d);

    for (size_t i = 0; i < n; i++) {
        const SCL_ML_FLOAT *row = &ds->data[i * stride];
        for (size_t k = 0; k < n_comp; k++)
            out[i * n_comp + k] = (SCL_ML_FLOAT)(
                (double)scl_ml_simd.dot_f(row, &model->components[k * d], d) -
                (double)mean_dot[k]);
    }

    if (model->params.whiten) {
        for (size_t k = 0; k < n_comp; k++) {
            SCL_ML_FLOAT inv_std = 1.0f / sqrtf(model->explained_variance[k] + SCL_ML_EPSILON);
            for (size_t i = 0; i < n; i++)
                out[i * n_comp + k] *= inv_std;
        }
    }

    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_pca_fit_transform(scl_ml_pca_t *model, const scl_ml_dataset_t *ds,
                          SCL_ML_FLOAT *out) {
    scl_error_t err = scl_ml_pca_fit(model, ds);
    if (scl_unlikely(err != SCL_OK)) return err;
    return scl_ml_pca_transform(model, ds, out);
}

SCL_WARN_UNUSED scl_error_t
scl_ml_pca_inverse_transform(scl_ml_pca_t *model,
                              const SCL_ML_FLOAT *X_proj, size_t n_samples,
                              SCL_ML_FLOAT *out) {
    if (scl_unlikely(!model || !X_proj || !out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;

    size_t d = model->n_features;
    size_t n_comp = model->n_components;

    /* out = X_proj @ components + mean.  components is [n_comp x d] row-major,
     * exactly the B matrix GEMM expects: C[n,d] = A[n,n_comp] @ B[n_comp,d]. */
    scl_ml_simd.gemm(out, X_proj, model->components,
                     n_samples, d, n_comp, 1.0f, 0.0f);
    for (size_t i = 0; i < n_samples; i++)
        for (size_t j = 0; j < d; j++)
            out[i * d + j] += model->mean[j];

    return SCL_OK;
}

SCL_PURE size_t
scl_ml_pca_get_n_components(const scl_ml_pca_t *model) {
    return model ? model->n_components : 0;
}

SCL_WARN_UNUSED const SCL_ML_FLOAT *
scl_ml_pca_get_components(const scl_ml_pca_t *model) {
    return model ? model->components : NULL;
}

SCL_WARN_UNUSED const SCL_ML_FLOAT *
scl_ml_pca_get_mean(const scl_ml_pca_t *model) {
    return model ? model->mean : NULL;
}

SCL_WARN_UNUSED const SCL_ML_FLOAT *
scl_ml_pca_get_explained_variance(const scl_ml_pca_t *model) {
    return model ? model->explained_variance : NULL;
}

SCL_WARN_UNUSED const SCL_ML_FLOAT *
scl_ml_pca_get_explained_variance_ratio(const scl_ml_pca_t *model) {
    return model ? model->explained_variance_ratio : NULL;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_pca_save(const scl_ml_pca_t *model,
                 uint8_t **buf, size_t *len, scl_allocator_t *alloc) {
    if (scl_unlikely(!model || !buf || !len)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!model->fitted)) return SCL_ERR_INVALID_STATE;
    if (scl_unlikely(!alloc)) return SCL_ERR_NULL_PTR;

    size_t d = model->n_features;
    size_t nc = model->n_components;

    scl_ml_serial_header_t hdr;
    hdr.magic     = SCL_ML_MAGIC;
    hdr.version   = SCL_ML_FORMAT_VERSION;
    hdr.algo_id   = SCL_ML_ALGO_PCA;
    hdr.header_sz = sizeof(hdr);
    hdr.reserved  = 0;

    size_t payload_sz = sizeof(size_t) + sizeof(size_t) +
                        d * sizeof(SCL_ML_FLOAT) +
                        nc * d * sizeof(SCL_ML_FLOAT) +
                        nc * sizeof(SCL_ML_FLOAT) +
                        nc * sizeof(SCL_ML_FLOAT);
    size_t total = sizeof(hdr) + payload_sz + sizeof(uint32_t);

    uint8_t *buffer = (uint8_t *)scl_calloc(alloc, 1, total, alignof(max_align_t));
    if (!buffer) return SCL_ERR_OUT_OF_MEMORY;

    memcpy(buffer, &hdr, sizeof(hdr));
    size_t off = sizeof(hdr);

    memcpy(buffer + off, &d, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, &nc, sizeof(size_t)); off += sizeof(size_t);
    memcpy(buffer + off, model->mean, d * sizeof(SCL_ML_FLOAT)); off += d * sizeof(SCL_ML_FLOAT);
    memcpy(buffer + off, model->components, nc * d * sizeof(SCL_ML_FLOAT));
    off += nc * d * sizeof(SCL_ML_FLOAT);
    memcpy(buffer + off, model->explained_variance, nc * sizeof(SCL_ML_FLOAT));
    off += nc * sizeof(SCL_ML_FLOAT);
    memcpy(buffer + off, model->explained_variance_ratio, nc * sizeof(SCL_ML_FLOAT));
    off += nc * sizeof(SCL_ML_FLOAT);

    uint32_t crc = scl_ml_crc32c(buffer + sizeof(scl_ml_serial_header_t), payload_sz);
    memcpy(buffer + off, &crc, sizeof(crc));

    *buf = buffer;
    *len = total;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_pca_load(scl_ml_pca_t **model,
                 const uint8_t *buf, size_t len,
                 scl_ml_pca_params_t params) {
    if (scl_unlikely(!model || !buf)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(len < sizeof(scl_ml_serial_header_t) + sizeof(uint32_t)))
        return SCL_ERR_INVALID_ARG;

    const scl_ml_serial_header_t *hdr = (const scl_ml_serial_header_t *)buf;
    if (scl_unlikely(hdr->magic != SCL_ML_MAGIC || hdr->algo_id != SCL_ML_ALGO_PCA))
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
    size_t d = 0, nc = 0;
    memcpy(&d, buf + off, sizeof(size_t)); off += sizeof(size_t);
    memcpy(&nc, buf + off, sizeof(size_t)); off += sizeof(size_t);

    params.n_components = nc;
    scl_ml_pca_t *m;
    scl_error_t err = scl_ml_pca_new(&m, params);
    if (err != SCL_OK) return err;

    m->n_features = d;
    m->n_components = nc;

    scl_allocator_t *a = m->alloc;
    m->mean = (SCL_ML_FLOAT *)scl_calloc(a, d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    m->components = (SCL_ML_FLOAT *)scl_calloc(a, nc * d, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    m->explained_variance = (SCL_ML_FLOAT *)scl_calloc(a, nc, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    m->explained_variance_ratio = (SCL_ML_FLOAT *)scl_calloc(a, nc, sizeof(SCL_ML_FLOAT), alignof(max_align_t));
    if (!m->mean || !m->components ||
        !m->explained_variance || !m->explained_variance_ratio) {
        scl_ml_pca_free(m);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    memcpy(m->mean, buf + off, d * sizeof(SCL_ML_FLOAT));
    off += d * sizeof(SCL_ML_FLOAT);
    memcpy(m->components, buf + off, nc * d * sizeof(SCL_ML_FLOAT));
    off += nc * d * sizeof(SCL_ML_FLOAT);
    memcpy(m->explained_variance, buf + off, nc * sizeof(SCL_ML_FLOAT));
    off += nc * sizeof(SCL_ML_FLOAT);
    memcpy(m->explained_variance_ratio, buf + off, nc * sizeof(SCL_ML_FLOAT));

    m->fitted = 1;
    *model = m;
    return SCL_OK;
}
