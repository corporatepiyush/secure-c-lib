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

/* ML dataset and common infrastructure implementation. */

#include "scl_ml.h"
#include "scl_ml_simd.h"
#include <string.h>
#include <math.h>
#include <float.h>

/* ── Helpers ─────────────────────────────────────────────────── */
static const size_t SCL_ML_ALIGNMENT = 32; /* AVX2 alignment requirement */

static SCL_COLD_PATH scl_error_t
scl_ml_check_nan(const SCL_ML_FLOAT *data, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (scl_unlikely(!isfinite(data[i])))
            return SCL_ERR_ML_MISSING_DATA;
    }
    return SCL_OK;
}

/* ── Dataset lifecycle ───────────────────────────────────────── */

SCL_WARN_UNUSED scl_error_t
scl_ml_dataset_init(scl_ml_dataset_t *ds, scl_allocator_t *alloc,
                     size_t n_rows, size_t n_cols) {
    if (scl_unlikely(!ds || !alloc)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(n_rows == 0 || n_cols == 0)) return SCL_ERR_INVALID_ARG;

    memset(ds, 0, sizeof(*ds));

    size_t row_stride = scl_ml_align_up(n_cols * sizeof(SCL_ML_FLOAT),
                                         SCL_ML_ALIGNMENT) / sizeof(SCL_ML_FLOAT);
    if (row_stride < n_cols) return SCL_ERR_SIZE_OVERFLOW;

    size_t total_bytes;
    if (scl_mul_overflow(n_rows, row_stride * sizeof(SCL_ML_FLOAT), &total_bytes))
        return SCL_ERR_SIZE_OVERFLOW;

    ds->data = (SCL_ML_FLOAT *)scl_alloc(alloc, total_bytes, SCL_ML_ALIGNMENT);
    if (scl_unlikely(!ds->data)) return SCL_ERR_OUT_OF_MEMORY;

    size_t target_bytes;
    if (scl_mul_overflow(n_rows, sizeof(SCL_ML_FLOAT), &target_bytes))
        return SCL_ERR_SIZE_OVERFLOW;
    ds->targets = (SCL_ML_FLOAT *)scl_alloc(alloc, target_bytes, SCL_ML_ALIGNMENT);
    if (scl_unlikely(!ds->targets)) {
        scl_free(alloc, ds->data);
        ds->data = NULL;
        return SCL_ERR_OUT_OF_MEMORY;
    }

    ds->n_rows     = n_rows;
    ds->n_cols     = n_cols;
    ds->row_stride = row_stride;
    ds->owns_data  = 1;
    ds->owns_targets = 1;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_dataset_wrap(scl_ml_dataset_t *ds, SCL_ML_FLOAT *data,
                     SCL_ML_FLOAT *targets,
                     size_t n_rows, size_t n_cols) {
    if (scl_unlikely(!ds)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!data || n_rows == 0 || n_cols == 0))
        return SCL_ERR_INVALID_ARG;

    memset(ds, 0, sizeof(*ds));
    ds->data       = data;
    ds->targets    = targets;
    ds->n_rows     = n_rows;
    ds->n_cols     = n_cols;
    ds->row_stride = n_cols; /* tightly packed unless user overrides */
    ds->owns_data  = 0;
    ds->owns_targets = (targets != NULL) ? 0 : 1;
    return SCL_OK;
}

SCL_WARN_UNUSED scl_error_t
scl_ml_dataset_prepare(scl_ml_dataset_t *ds, scl_allocator_t *alloc) {
    if (scl_unlikely(!ds)) return SCL_ERR_NULL_PTR;

    /* Validate no NaN/Inf in data */
    size_t total = ds->n_rows * ds->row_stride;
    scl_error_t err = scl_ml_check_nan(ds->data, total);
    if (scl_unlikely(err != SCL_OK)) return err;

    if (ds->targets) {
        err = scl_ml_check_nan(ds->targets, ds->n_rows);
        if (scl_unlikely(err != SCL_OK)) return err;
    }

    /* Build column-major view if not already present and allocator given */
    if (!ds->data_col && alloc) {
        size_t col_stride = scl_ml_align_up(ds->n_rows * sizeof(SCL_ML_FLOAT),
                                              SCL_ML_ALIGNMENT) / sizeof(SCL_ML_FLOAT);
        size_t col_bytes;
        if (scl_mul_overflow(ds->n_cols, col_stride * sizeof(SCL_ML_FLOAT), &col_bytes))
            return SCL_ERR_SIZE_OVERFLOW;

        ds->data_col = (SCL_ML_FLOAT *)scl_alloc(alloc, col_bytes, SCL_ML_ALIGNMENT);
        if (scl_unlikely(!ds->data_col)) return SCL_ERR_OUT_OF_MEMORY;

        /* Transpose row-major to column-major */
        for (size_t i = 0; i < ds->n_rows; i++)
            for (size_t j = 0; j < ds->n_cols; j++)
                ds->data_col[j * col_stride + i] = ds->data[i * ds->row_stride + j];

        ds->col_stride  = col_stride;
        ds->owns_col    = 1;
    }

    return SCL_OK;
}

void
scl_ml_dataset_destroy(scl_ml_dataset_t *ds, scl_allocator_t *alloc) {
    if (scl_unlikely(!ds)) return;
    if (ds->owns_data && ds->data)
        scl_free(alloc, ds->data);
    if (ds->owns_targets && ds->targets)
        scl_free(alloc, ds->targets);
    if (ds->owns_col && ds->data_col)
        scl_free(alloc, ds->data_col);
    memset(ds, 0, sizeof(*ds));
}

SCL_WARN_UNUSED bool
scl_ml_dataset_has_missing(const scl_ml_dataset_t *ds) {
    if (scl_unlikely(!ds || !ds->data)) return true;
    size_t total = ds->n_rows * ds->row_stride;
    for (size_t i = 0; i < total; i++)
        if (scl_unlikely(!isfinite(ds->data[i])))
            return true;
    if (ds->targets) {
        for (size_t i = 0; i < ds->n_rows; i++)
            if (scl_unlikely(!isfinite(ds->targets[i])))
                return true;
    }
    return false;
}

/* ── CRC32C (Castagnoli polynomial 0x1EDC6F41) ────────────────── */
uint32_t
scl_ml_crc32c(const void *data, size_t len) {
    static const uint32_t table[256] = {
        0x00000000u, 0xF26B8303u, 0xE13B70F7u, 0x1350F3F4u,
        0xC79A971Fu, 0x35F1141Cu, 0x26A1E7E8u, 0xD4CA64EBu,
        0x8AD958CFu, 0x78B2DBCCu, 0x6BE22838u, 0x9989AB3Bu,
        0x4D43CF50u, 0xBF284C53u, 0xAC78BFA7u, 0x5E133CA4u,
        0x105EC76Fu, 0xE235446Cu, 0xF165B798u, 0x030E349Bu,
        0xD7C450F0u, 0x25AFD3F3u, 0x36FF2007u, 0xC494A304u,
        0x9A879F20u, 0x68EC1C23u, 0x7BBCEFD7u, 0x89D76CD4u,
        0x5D1D08BFu, 0xAF768BBCu, 0xBC267848u, 0x4E4DFB4Bu,
        0x20BD8EDEu, 0xD2D60DDDu, 0xC186FE29u, 0x33ED7D2Au,
        0xE7271941u, 0x154C9A42u, 0x061C69B6u, 0xF477EAB5u,
        0xAA64D691u, 0x580F5592u, 0x4B5FA666u, 0xB9342565u,
        0x6DFE410Eu, 0x9F95C20Du, 0x8CC531F9u, 0x7EAEB2FAu,
        0x30E34931u, 0xC288CA32u, 0xD1D839C6u, 0x23B3BAC5u,
        0xF779DEAEu, 0x05125DADu, 0x1642AE59u, 0xE4292D5Au,
        0xBA3A117Eu, 0x4851927Du, 0x5B016189u, 0xA96AE28Au,
        0x7DA086E1u, 0x8FCB05E2u, 0x9C9BF616u, 0x6EF07515u,
        0x417B1DBCu, 0xB3109EBFu, 0xA0406D4Bu, 0x522BEE48u,
        0x86E18A23u, 0x748A0920u, 0x67DAFAD4u, 0x95B179D7u,
        0xCBA245F3u, 0x39C9C6F0u, 0x2A993504u, 0xD8F2B607u,
        0x0C38D26Cu, 0xFE53516Fu, 0xED03A29Bu, 0x1F682198u,
        0x5125DA53u, 0xA34E5950u, 0xB01EAAA4u, 0x427529A7u,
        0x96BF4DCCu, 0x64D4CECFu, 0x77843D3Bu, 0x85EFBE38u,
        0xDBFC821Cu, 0x2997011Fu, 0x3AC7F2EBu, 0xC8AC71E8u,
        0x1C661583u, 0xEE0D9680u, 0xFD5D6574u, 0x0F36E677u,
        0x61C693E2u, 0x93AD10E1u, 0x80FDE315u, 0x72966016u,
        0xA65C047Du, 0x5437877Eu, 0x4767748Au, 0xB50CF789u,
        0xEB1FCBADu, 0x197448AEu, 0x0A24BB5Au, 0xF84F3859u,
        0x2C855C32u, 0xDEEEDF31u, 0xCDBE2CC5u, 0x3FD5AFC6u,
        0x7198540Du, 0x83F3D70Eu, 0x90A324FAu, 0x62C8A7F9u,
        0xB602C392u, 0x44694091u, 0x5739B365u, 0xA5523066u,
        0xFB410C42u, 0x092A8F41u, 0x1A7A7CB5u, 0xE811FFB6u,
        0x3CDB9BDDu, 0xCEB018DEu, 0xDDE0EB2Au, 0x2F8B6829u,
        0x82F63B78u, 0x709DB87Bu, 0x63CD4B8Fu, 0x91A6C88Cu,
        0x456CACE7u, 0xB7072FE4u, 0xA457DC10u, 0x563C5F13u,
        0x082F6337u, 0xFA44E034u, 0xE91413C0u, 0x1B7F90C3u,
        0xCFB5F4A8u, 0x3DDE77ABu, 0x2E8E845Fu, 0xDCE5075Cu,
        0x92A8FC97u, 0x60C37F94u, 0x73938C60u, 0x81F80F63u,
        0x55326B08u, 0xA759E80Bu, 0xB4091BFFu, 0x466298FCu,
        0x1871A4D8u, 0xEA1A27DBu, 0xF94AD42Fu, 0x0B21572Cu,
        0xDFEB3347u, 0x2D80B044u, 0x3ED043B0u, 0xCCBBC0B3u,
        0xA24BB526u, 0x50203625u, 0x4370C5D1u, 0xB11B46D2u,
        0x65D122B9u, 0x97BAA1BAu, 0x84EA524Eu, 0x7681D14Du,
        0x2892ED69u, 0xDAF96E6Au, 0xC9A99D9Eu, 0x3BC21E9Du,
        0xEF087AF6u, 0x1D63F9F5u, 0x0E330A01u, 0xFC588902u,
        0xB21572C9u, 0x407EF1CAu, 0x532E023Eu, 0xA145813Du,
        0x758FE556u, 0x87E46655u, 0x94B495A1u, 0x66DF16A2u,
        0x38CC2A86u, 0xCAA7A985u, 0xD9F75A71u, 0x2B9CD972u,
        0xFF56BD19u, 0x0D3D3E1Au, 0x1E6DCDEEu, 0xEC064EEDu,
        0xC38D2644u, 0x31E6A547u, 0x22B656B3u, 0xD0DDD5B0u,
        0x0417B1DBu, 0xF67C32D8u, 0xE52CC12Cu, 0x1747422Fu,
        0x49547E0Bu, 0xBB3FFD08u, 0xA86F0EFCu, 0x5A048DFFu,
        0x8ECEE994u, 0x7CA56A97u, 0x6FF59963u, 0x9D9E1A60u,
        0xD3D3E1ABu, 0x21B862A8u, 0x32E8915Cu, 0xC083125Fu,
        0x14497634u, 0xE622F537u, 0xF57206C3u, 0x071985C0u,
        0x590AB9E4u, 0xAB613AE7u, 0xB831C913u, 0x4A5A4A10u,
        0x9E902E7Bu, 0x6CFBAD78u, 0x7FAB5E8Cu, 0x8DC0DD8Fu,
        0xE330A81Au, 0x115B2B19u, 0x020BD8EDu, 0xF0605BEEu,
        0x24AA3F85u, 0xD6C1BC86u, 0xC5914F72u, 0x37FACC71u,
        0x69E9F055u, 0x9B827356u, 0x88D280A2u, 0x7AB903A1u,
        0xAE7367CAu, 0x5C18E4C9u, 0x4F48173Du, 0xBD23943Eu,
        0xF36E6FF5u, 0x0105ECF6u, 0x12551F02u, 0xE03E9C01u,
        0x34F4F86Au, 0xC69F7B69u, 0xD5CF889Du, 0x27A40B9Eu,
        0x79B737BAu, 0x8BDCB4B9u, 0x988C474Du, 0x6AE7C44Eu,
        0xBE2DA025u, 0x4C462326u, 0x5F16D0D2u, 0xAD7D53D1u,
    };
    uint32_t crc = 0xFFFFFFFFu;
    const unsigned char *p = (const unsigned char *)data;
    for (size_t i = 0; i < len; i++)
        crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}
