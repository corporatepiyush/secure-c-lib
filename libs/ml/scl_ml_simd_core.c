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

/* SIMD dispatch core: CPU feature detection, dispatch table, init.
 * The per-ISA .c files (avx2, avx512, neon, sve) provide override
 * functions that selectively fill in the dispatch table.
 *
 * Coverage: x86-64 (CPUID), ARM64 Linux (getauxval), ARM64 macOS (sysctl).
 */

#include "scl_ml_simd.h"
#include "scl_pthread.h"
#include "scl_stdlib.h"
#include <string.h>

/* Global dispatch table — always fully populated via scalar fallback first. */
scl_ml_simd_t scl_ml_simd;
static scl_once_t scl_ml_simd_once = SCL_ONCE_INIT;

/* ══════════════════════════════════════════════════════════════════
 * x86-64 CPUID-based feature detection
 * ══════════════════════════════════════════════════════════════════ */

#if defined(SCL_ARCH_X86_64)

static inline void scl_ml_cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax,
                                uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
#if defined(__GNUC__) || defined(__clang__)
  __asm__ volatile("cpuid"
                   : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                   : "a"(leaf), "c"(subleaf)
                   : "memory");
#else
  *eax = *ebx = *ecx = *edx = 0;
  (void)leaf;
  (void)subleaf;
#endif
}

uint64_t scl_ml_cpu_features(void) {
  uint64_t features = 0;
  uint32_t eax, ebx, ecx, edx;

  /* Leaf 1: detect SSE4.2 */
  scl_ml_cpuid(1, 0, &eax, &ebx, &ecx, &edx);
  if (ecx & (1u << 19))
    features |= SCL_ML_CPU_SSE42;

  /* Leaf 7, subleaf 0: detect AVX2 and AVX-512 */
  scl_ml_cpuid(7, 0, &eax, &ebx, &ecx, &edx);
  if (ebx & (1u << 5))
    features |= SCL_ML_CPU_AVX2;
  if (ebx & (1u << 16))
    features |= SCL_ML_CPU_AVX512F;
  if (ebx & (1u << 17))
    features |= SCL_ML_CPU_AVX512DQ;
  if (ebx & (1u << 30))
    features |= SCL_ML_CPU_AVX512BW;

  /* Sanity: AVX-512 sub-features require AVX-512F */
  if (!(features & SCL_ML_CPU_AVX512F)) {
    features &= ~(uint64_t)(SCL_ML_CPU_AVX512BW | SCL_ML_CPU_AVX512DQ);
  }

  return features;
}

/* ══════════════════════════════════════════════════════════════════
 * ARM64 feature detection
 * ══════════════════════════════════════════════════════════════════ */

#elif defined(SCL_ARCH_ARM64)

#if defined(__linux__)
#include <sys/auxv.h>
#ifndef AT_HWCAP
#define AT_HWCAP 16
#endif
#ifndef HWCAP_ASIMD
#define HWCAP_ASIMD (1 << 1)
#endif
#ifndef HWCAP_SVE
#define HWCAP_SVE (1 << 22)
#endif

uint64_t scl_ml_cpu_features(void) {
  uint64_t features = 0;
  unsigned long hwcap = getauxval(AT_HWCAP);
  if (hwcap & HWCAP_ASIMD)
    features |= SCL_ML_CPU_NEON;
  if (hwcap & HWCAP_SVE)
    features |= SCL_ML_CPU_SVE;
  return features;
}

#elif defined(__APPLE__)
#include <sys/sysctl.h>

uint64_t scl_ml_cpu_features(void) {
  uint64_t features = 0;
  /* All Apple Silicon has NEON. */
  features |= SCL_ML_CPU_NEON;
  /* Check for SVE (M1 Ultra / M2 and later have SVE). */
  int has_sve = 0;
  size_t len = sizeof(has_sve);
  if (sysctlbyname("hw.optional.arm.FEAT_SVE", &has_sve, &len, NULL, 0) == 0) {
    if (has_sve)
      features |= SCL_ML_CPU_SVE;
  }
  return features;
}

#else
/* Generic ARM64 fallback — assume NEON (all ARMv8+ have it). */
uint64_t scl_ml_cpu_features(void) { return SCL_ML_CPU_NEON; }
#endif

/* ══════════════════════════════════════════════════════════════════
 * Other architectures — no SIMD features reported
 * ══════════════════════════════════════════════════════════════════ */

#else
uint64_t scl_ml_cpu_features(void) { return 0; }
#endif

/* ══════════════════════════════════════════════════════════════════
 * shared SIMD helpers (used across multiple ISA backends)
 * ══════════════════════════════════════════════════════════════════ */

/* Blade: pairwise reduction 4→1 within a 128-bit lane (x86 only). */
#if defined(SCL_ARCH_X86_64)
static inline float scl_simd_hsum_128(__m128 v) {
  v = _mm_add_ps(v, _mm_movehl_ps(v, v));
  v = _mm_add_ss(v, _mm_shuffle_ps(v, v, 1));
  return _mm_cvtss_f32(v);
}
#endif

/* ══════════════════════════════════════════════════════════════════
 * Dispatch initialization
 * Priority (applied lowest-first, later overrides replace):
 *   Scalar → SSE4.2 → NEON → AVX2+FMA → AVX-512 → SVE
 * The final result holds the best available slot for each kernel.
 * ══════════════════════════════════════════════════════════════════ */

static void scl_ml_simd_init_impl(void) {
  /* 1. Start with the universal scalar fallback. */
  scl_ml_simd_override_scalar(&scl_ml_simd);

  /* 2. Detect features. */
  uint64_t caps = scl_ml_cpu_features();

  /* 3. Install best available — order matters.
   *    Each override only fills non-NULL slots,
   *    so higher-ISA slots overwrite lower ones. */
  if (caps & SCL_ML_CPU_SSE42)
    scl_ml_simd_override_sse42(&scl_ml_simd);

  if (caps & SCL_ML_CPU_NEON)
    scl_ml_simd_override_neon(&scl_ml_simd);

#if defined(__ARM_FEATURE_SVE)
  if (caps & SCL_ML_CPU_SVE)
    scl_ml_simd_override_sve(&scl_ml_simd);
#endif

  if (caps & SCL_ML_CPU_AVX2)
    scl_ml_simd_override_avx2(&scl_ml_simd);

  if ((caps & SCL_ML_CPU_AVX512F) == SCL_ML_CPU_AVX512F)
    scl_ml_simd_override_avx512(&scl_ml_simd);
}

void scl_ml_simd_init(void) {
  scl_once(&scl_ml_simd_once, scl_ml_simd_init_impl);
}