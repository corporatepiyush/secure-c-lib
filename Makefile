MAKEFLAGS += -j$(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
CC       = gcc

#Detect compiler family
CC_IS_GCC := $(if $(findstring gcc,$(shell $(CC) --version 2>/dev/null | head -1 | tr '[:upper:]' '[:lower:]')),1,0)
CC_IS_CLANG := $(if $(findstring clang,$(shell $(CC) --version 2>/dev/null | head -1 | tr '[:upper:]' '[:lower:]')),1,0)

#Base hardening(always on)
HARDEN_CFLAGS  = -std=c17 -Wall -Wextra -Wpedantic -Werror \
                 -O2 \
                 -fstack-protector-strong \
                 -fPIE \
                 -Wformat -Wformat-security \
                 -Wnull-dereference

#GCC - only hardening
ifeq ($(CC_IS_GCC),1)
HARDEN_CFLAGS += -fstack-clash-protection \
                 -Walloc-zero \
                 -Wstringop-overflow \
                 -Warray-bounds=2
HARDEN_LDFLAGS = -Wl,-z,relro,-z,now
else
HARDEN_LDFLAGS =
endif

ifneq ($(shell uname),Darwin)
HARDEN_LDFLAGS += -pie
endif

#Detect glibc 2.35 + for _FORTIFY_SOURCE = 3
FORTIFY := 2
ifneq ($(shell $(CC) -dM -E - </dev/null 2>/dev/null | grep __GLIBC__ | awk '{print $$NF}'),)
GLIBC_VER := $(shell $(CC) -dM -E - </dev/null 2>/dev/null | grep __GLIBC_MINOR__ | awk '{print $$NF}')
ifeq ($(shell test $(GLIBC_VER) -ge 35 && echo 1),1)
FORTIFY := 3
endif
endif
#Undefine any pre - existing _FORTIFY_SOURCE, then set our level
HARDEN_CFLAGS += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=$(FORTIFY)

#Control - flow integrity(x86 - 64 GCC only)
ifeq ($(CC_IS_GCC),1)
ifneq (,$(filter x86_64%,$(shell $(CC) -dumpmachine)))
HARDEN_CFLAGS += -fcf-protection=full
endif
endif

CFLAGS   = $(HARDEN_CFLAGS)
#ML - specific flags : -O3, fma contraction, auto - vectorization
ML_CFLAGS = -O3 -ffp-contract=fast -funroll-loops -ftree-vectorize
LDFLAGS  = $(HARDEN_LDFLAGS) -lm -lpthread
ARFLAGS  = rcs

#All header directories live under libs /
LIBDIRS  := $(shell find ./libs -maxdepth 4 -type d -not -path '*/.git/*' | sort -u)

#Sequential library sources(all scl_ *.c under libs /, excluding test files).
#NOTE : SIMD dispatch files are excluded here — they are compiled separately
#with ISA - specific flags and linked into the final library.
LIBSRCS  := $(shell find ./libs -name 'scl_*.c' -not -name 'test_*.c' \
		-not -path '*/.git/*' \
		-not -path '*/ml/*simd_core*' \
		-not -path '*/ml/*simd_scalar*' \
		-not -path '*/ml/*simd_sse42*' \
		-not -path '*/ml/*simd_avx2*' \
		-not -path '*/ml/*simd_avx512*' \
		-not -path '*/ml/*simd_neon*' \
		-not -path '*/ml/*simd_sve*' \
		-not -path '*/ml/*simd_avx*')
LIBOBJS  := $(patsubst %.c, build/%.o, $(LIBSRCS))
LIBNAME  = libscl.a

# ── ML SIMD sources(compiled with ISA - specific flags) ────────────
#Scalar fallback — always compiled, no special flags
ML_SIMD_SCALAR_SRCS := libs/ml/scl_ml_simd_scalar.c
ML_SIMD_CORE_SRCS   := libs/ml/scl_ml_simd_core.c

#ISA - specific modules(compiled only when host supports them)
ML_SIMD_SSE42_SRCS  :=
ML_SIMD_AVX2_SRCS   :=
ML_SIMD_AVX512_SRCS :=
ML_SIMD_NEON_SRCS   :=
ML_SIMD_SVE_SRCS    :=

ifeq ($(HOST_IS_X86),1)
  ML_SIMD_SSE42_SRCS  := libs/ml/scl_ml_simd_sse42.c
  ML_SIMD_AVX2_SRCS   := libs/ml/scl_ml_simd_avx2.c
  ML_SIMD_AVX512_SRCS := libs/ml/scl_ml_simd_avx512.c
endif
ifeq ($(HOST_IS_ARM64),1)
  ML_SIMD_NEON_SRCS   := libs/ml/scl_ml_simd_neon.c
#SVE requires GCC with full ACLE support(Linux only)
  ifneq ($(shell uname),Darwin)
    ML_SIMD_SVE_SRCS    := libs/ml/scl_ml_simd_sve.c
  endif
endif

ML_SIMD_SRCS := $(ML_SIMD_SCALAR_SRCS) $(ML_SIMD_CORE_SRCS) \
		$(ML_SIMD_SSE42_SRCS) $(ML_SIMD_AVX2_SRCS) \
		$(ML_SIMD_AVX512_SRCS) $(ML_SIMD_NEON_SRCS) \
		$(ML_SIMD_SVE_SRCS)
ML_SIMD_OBJS := $(patsubst %.c, build/%.o, $(ML_SIMD_SRCS))

#Concurrent library sources(all scl_concurrent_ *.c under libs /)
CONC_SRCS := $(shell find ./libs -name 'scl_concurrent_*.c' -not -path '*/.git/*')
CONC_OBJS := $(patsubst %.c, build/%.o, $(CONC_SRCS))
CONC_LIBNAME = libscl_concurrent.a

#Test binaries(live in./ tests /)
TESTSRCS := $(shell find ./tests -name 'test_*.c' -not -path '*/.git/*')
TESTBINS := $(patsubst %.c, build/%_bin, $(TESTSRCS))

#- I flags for every directory under libs /
INCFLAGS := $(addprefix -I, $(LIBDIRS))

#Fuzz targets(live in./ tests / fuzz /)
FUZZSRCS := $(wildcard tests/fuzz/*.c)
FUZZBINS := $(patsubst %.c, build/%_fuzzbin, $(FUZZSRCS))

.PHONY: all lib test clean check asan tsan ubsan msan sanitize-all fuzz filc llvm

# Mandatory sanitizer checks before shipping
sanitize-all: asan-check ubsan-check

# Default build: requires sanitizers before tests
all: lib sanitize-all test

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCFLAGS) -I$(dir $<) -MMD -MP -c $< -o $@

-include $(patsubst %.o, %.d, $(LIBOBJS))

# ML files get extra optimization flags
build/libs/ml/%.o: libs/ml/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ML_CFLAGS) $(INCFLAGS) -Ilibs/ml -I$(dir $<) -MMD -MP -c $< -o $@

-include $(patsubst build/libs/ml/%.o, build/libs/ml/%.d, $(filter build/libs/ml/%.o, $(LIBOBJS)))

build/libs/ml/preprocessing/%.o: libs/ml/preprocessing/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ML_CFLAGS) $(INCFLAGS) -Ilibs/ml -I$(dir $<) -MMD -MP -c $< -o $@

-include $(patsubst build/libs/ml/preprocessing/%.o, build/libs/ml/preprocessing/%.d, $(filter build/libs/ml/preprocessing/%.o, $(LIBOBJS)))

build/libs/ml/linear_model/%.o: libs/ml/linear_model/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ML_CFLAGS) $(INCFLAGS) -Ilibs/ml -I$(dir $<) -MMD -MP -c $< -o $@

-include $(patsubst build/libs/ml/linear_model/%.o, build/libs/ml/linear_model/%.d, $(filter build/libs/ml/linear_model/%.o, $(LIBOBJS)))

build/libs/ml/tree/%.o: libs/ml/tree/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ML_CFLAGS) $(INCFLAGS) -Ilibs/ml -I$(dir $<) -MMD -MP -c $< -o $@

-include $(patsubst build/libs/ml/tree/%.o, build/libs/ml/tree/%.d, $(filter build/libs/ml/tree/%.o, $(LIBOBJS)))

build/libs/ml/svm/%.o: libs/ml/svm/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ML_CFLAGS) $(INCFLAGS) -Ilibs/ml -I$(dir $<) -MMD -MP -c $< -o $@

-include $(patsubst build/libs/ml/svm/%.o, build/libs/ml/svm/%.d, $(filter build/libs/ml/svm/%.o, $(LIBOBJS)))

build/libs/ml/neighbors/%.o: libs/ml/neighbors/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ML_CFLAGS) $(INCFLAGS) -Ilibs/ml -I$(dir $<) -MMD -MP -c $< -o $@

-include $(patsubst build/libs/ml/neighbors/%.o, build/libs/ml/neighbors/%.d, $(filter build/libs/ml/neighbors/%.o, $(LIBOBJS)))

build/libs/ml/naive_bayes/%.o: libs/ml/naive_bayes/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ML_CFLAGS) $(INCFLAGS) -Ilibs/ml -I$(dir $<) -MMD -MP -c $< -o $@

-include $(patsubst build/libs/ml/naive_bayes/%.o, build/libs/ml/naive_bayes/%.d, $(filter build/libs/ml/naive_bayes/%.o, $(LIBOBJS)))

build/libs/ml/cluster/%.o: libs/ml/cluster/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ML_CFLAGS) $(INCFLAGS) -Ilibs/ml -I$(dir $<) -MMD -MP -c $< -o $@

-include $(patsubst build/libs/ml/cluster/%.o, build/libs/ml/cluster/%.d, $(filter build/libs/ml/cluster/%.o, $(LIBOBJS)))

build/libs/ml/decomposition/%.o: libs/ml/decomposition/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ML_CFLAGS) $(INCFLAGS) -Ilibs/ml -I$(dir $<) -MMD -MP -c $< -o $@

-include $(patsubst build/libs/ml/decomposition/%.o, build/libs/ml/decomposition/%.d, $(filter build/libs/ml/decomposition/%.o, $(LIBOBJS)))

# ── Host architecture detection ────────────────────────────────────
HOST_ARCH := $(shell uname -m)
HOST_IS_X86   := $(if $(filter x86_64%,$(HOST_ARCH)),1,0)
HOST_IS_ARM64 := $(if $(filter arm64% aarch64%,$(HOST_ARCH)),1,0)

# ── SIMD dispatch: scalar (always compiled) ──────────────────────
build/libs/ml/scl_ml_simd_scalar.o: libs/ml/scl_ml_simd_scalar.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ML_CFLAGS) $(INCFLAGS) -Ilibs/ml -I$(dir $<) -MMD -MP -c $< -o $@

build/libs/ml/scl_ml_simd_core.o: libs/ml/scl_ml_simd_core.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ML_CFLAGS) $(INCFLAGS) -Ilibs/ml -I$(dir $<) -MMD -MP -c $< -o $@

# ── SIMD: SSE4.2 (x86-64 only) ───────────────────────────────────
ifeq ($(HOST_IS_X86),1)
build/libs/ml/scl_ml_simd_sse42.o: libs/ml/scl_ml_simd_sse42.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ML_CFLAGS) -msse4.2 $(INCFLAGS) -Ilibs/ml -I$(dir $<) -MMD -MP -c $< -o $@

build/libs/ml/scl_ml_simd_avx2.o: libs/ml/scl_ml_simd_avx2.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ML_CFLAGS) -mavx2 -mfma $(INCFLAGS) -Ilibs/ml -I$(dir $<) -MMD -MP -c $< -o $@

build/libs/ml/scl_ml_simd_avx512.o: libs/ml/scl_ml_simd_avx512.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ML_CFLAGS) -mavx512f -mavx512bw -mavx512dq $(INCFLAGS) -Ilibs/ml -I$(dir $<) -MMD -MP -c $< -o $@
ML_SIMD_OBJS += build/libs/ml/scl_ml_simd_sse42.o build/libs/ml/scl_ml_simd_avx2.o build/libs/ml/scl_ml_simd_avx512.o
endif

# ── SIMD: ARM NEON (ARM64 only) ─────────────────────────────────
ifeq ($(HOST_IS_ARM64),1)
build/libs/ml/scl_ml_simd_neon.o: libs/ml/scl_ml_simd_neon.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ML_CFLAGS) -march=armv8-a+simd $(INCFLAGS) -Ilibs/ml -I$(dir $<) -MMD -MP -c $< -o $@

# SVE requires full ACLE support (Linux GCC); Apple clang lacks SVE2 intrinsics
ifeq ($(shell uname),Linux)
build/libs/ml/scl_ml_simd_sve.o: libs/ml/scl_ml_simd_sve.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ML_CFLAGS) -march=armv8-a+sve $(INCFLAGS) -Ilibs/ml -I$(dir $<) -MMD -MP -c $< -o $@
ML_SIMD_OBJS += build/libs/ml/scl_ml_simd_sve.o
endif

ML_SIMD_OBJS += build/libs/ml/scl_ml_simd_neon.o
endif

$(LIBNAME): $(LIBOBJS) $(ML_SIMD_OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(CONC_LIBNAME): $(CONC_OBJS)
	$(AR) $(ARFLAGS) $@ $^

lib: $(LIBNAME) $(CONC_LIBNAME)

build/%_bin: %.c $(LIBNAME)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wno-unused-result $(INCFLAGS) -o $@ $< -L. -lscl $(LDFLAGS)

# Tests run in parallel via a recursive make that inherits the top-level
# -j jobserver. Each run-* target executes one binary and records failures
# as marker files so the summary step can count them after all finish.
TESTLOGDIR := build/testlogs
TESTRUNS   := $(addprefix run-,$(TESTBINS))

run-build/%_bin: build/%_bin
	@name=$$(basename $< _bin); \
	if ./$< > $(TESTLOGDIR)/$$name.log 2>&1; then \
		printf "%-50s✓ PASS\n" "$$name"; \
	else \
		printf "%-50s✗ FAIL\n" "$$name"; tail -5 $(TESTLOGDIR)/$$name.log; \
		touch $(TESTLOGDIR)/$$name.failed; \
	fi

test: $(TESTBINS)
	@rm -rf $(TESTLOGDIR); mkdir -p $(TESTLOGDIR)
	@$(MAKE) --no-print-directory -k $(TESTRUNS)
	@failed=$$(find $(TESTLOGDIR) -name '*.failed' | wc -l | tr -d ' '); \
	total=$(words $(TESTBINS)); \
	echo ""; echo "=== $$total tests: $$((total-failed)) passed, $$failed failed ==="; \
	[ $$failed -eq 0 ]

# ── Sanitizer targets ─────────────────────────────────────────
# Individual sanitizer targets (for detailed debugging)
asan:
	$(MAKE) clean
	$(MAKE) lib test CFLAGS="$(HARDEN_CFLAGS) -g -fsanitize=address -fno-omit-frame-pointer -fsanitize-recover=address" LDFLAGS="$(HARDEN_LDFLAGS) -lm -lpthread -fsanitize=address"

tsan:
	$(MAKE) clean
	$(MAKE) lib test CFLAGS="$(HARDEN_CFLAGS) -g -fsanitize=thread -fno-omit-frame-pointer" LDFLAGS="$(HARDEN_LDFLAGS) -lm -lpthread -fsanitize=thread"

ubsan:
	$(MAKE) clean
	$(MAKE) lib test CFLAGS="$(HARDEN_CFLAGS) -g -fsanitize=undefined -fno-omit-frame-pointer" LDFLAGS="$(HARDEN_LDFLAGS) -lm -lpthread -fsanitize=undefined"

msan:
	$(MAKE) clean
	$(MAKE) lib test CFLAGS="$(HARDEN_CFLAGS) -g -fsanitize=memory -fno-omit-frame-pointer -fsanitize-memory-track-origins" LDFLAGS="$(HARDEN_LDFLAGS) -lm -lpthread -fsanitize=memory"

# Sanitizer validation targets
asan-check:
	@echo "ASan: checking memory errors..."
	@$(MAKE) clean > /dev/null 2>&1
	@$(MAKE) lib test CFLAGS="$(HARDEN_CFLAGS) -g -fsanitize=address -fno-omit-frame-pointer -fsanitize-recover=address" LDFLAGS="$(HARDEN_LDFLAGS) -lm -lpthread -fsanitize=address"

ubsan-check:
	@echo "UBSan: checking undefined behavior..."
	@$(MAKE) clean > /dev/null 2>&1
	@$(MAKE) lib test CFLAGS="$(HARDEN_CFLAGS) -g -fsanitize=undefined -fno-omit-frame-pointer" LDFLAGS="$(HARDEN_LDFLAGS) -lm -lpthread -fsanitize=undefined"

tsan-check:
	@echo "TSan: checking data races..."
	@$(MAKE) clean > /dev/null 2>&1
	@$(MAKE) lib test CFLAGS="$(HARDEN_CFLAGS) -g -fsanitize=thread -fno-omit-frame-pointer" LDFLAGS="$(HARDEN_LDFLAGS) -lm -lpthread -fsanitize=thread"

# ── Fuzz targets (requires libFuzzer / -fsanitize=fuzzer) ────
build/%_fuzzbin: %.c $(LIBNAME)
	@mkdir -p $(dir $@)
	$(CC) $(HARDEN_CFLAGS) -g -fsanitize=fuzzer,address $(INCFLAGS) -o $@ $< -L. -lscl $(HARDEN_LDFLAGS) -lm -lpthread

fuzz: $(FUZZBINS)

# ── Fil-C (LLVM memory safety) profile ─────────────────────────
# Compiles with filclang which inserts invisible capabilities
# (InvisiCaps) via the GIMSO LLVM pass. Every pointer carries
# bounds, every load/store is checked, all UB is eliminated.
# -O3 + LTO offset capability overhead; Fil-C makes stack cookies,
# FORTIFY, and CFI redundant.
filc:
	$(MAKE) clean
	$(MAKE) lib test CC=filclang \
		CFLAGS="-std=c17 -Wall -Wextra -Wpedantic -Werror \
		        -O3 -flto=full \
		        -funroll-loops -ftree-vectorize -ffp-contract=fast \
		        -fvisibility=hidden" \
		LDFLAGS="-flto=full -lm -lpthread" \
		ML_CFLAGS=""

# ── LLVM (clang) native profile ─────────────────────────────────
# Maximum security + performance using vanilla clang. Includes
# stack variable initialization, LTO, and auto-vectorization.
# Drops Linux-only flags (-fstack-clash-protection, -fcf-protection,
# -Wl,-z,relro,-z,now) for cross-platform portability.
llvm:
	$(MAKE) clean
	$(MAKE) lib test CC=clang AR=llvm-ar \
		CFLAGS="-std=c17 -Wall -Wextra -Wpedantic -Werror \
		        -O3 -flto \
		        -fstack-protector-strong \
		        -ftrivial-auto-var-init=pattern \
		        -D_FORTIFY_SOURCE=2 \
		        -fvisibility=hidden \
		        -funroll-loops -ftree-vectorize -ffp-contract=fast \
		        -Wnull-dereference -Wformat -Wformat-security" \
		LDFLAGS="-flto -lm -lpthread" \
		ML_CFLAGS=""

check: sanitize-all tsan

clean:
	rm -f $(LIBNAME) $(CONC_LIBNAME)
	rm -rf build 2>/dev/null; true
