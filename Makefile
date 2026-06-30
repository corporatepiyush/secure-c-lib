MAKEFLAGS += -j$(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
CC       = gcc

# Detect compiler family
CC_IS_GCC := $(if $(findstring gcc,$(shell $(CC) --version 2>/dev/null | head -1 | tr '[:upper:]' '[:lower:]')),1,0)
CC_IS_CLANG := $(if $(findstring clang,$(shell $(CC) --version 2>/dev/null | head -1 | tr '[:upper:]' '[:lower:]')),1,0)

# Base hardening (always on)
HARDEN_CFLAGS  = -std=c17 -Wall -Wextra -Wpedantic -Werror \
                 -O2 \
                 -fstack-protector-strong \
                 -fPIE \
                 -Wformat -Wformat-security \
                 -Wnull-dereference

# GCC-only hardening
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

# Detect glibc 2.35+ for _FORTIFY_SOURCE=3
FORTIFY := 2
ifneq ($(shell $(CC) -dM -E - </dev/null 2>/dev/null | grep __GLIBC__ | awk '{print $$NF}'),)
GLIBC_VER := $(shell $(CC) -dM -E - </dev/null 2>/dev/null | grep __GLIBC_MINOR__ | awk '{print $$NF}')
ifeq ($(shell test $(GLIBC_VER) -ge 35 && echo 1),1)
FORTIFY := 3
endif
endif
# Undefine any pre-existing _FORTIFY_SOURCE, then set our level
HARDEN_CFLAGS += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=$(FORTIFY)

# Control-flow integrity (x86-64 GCC only)
ifeq ($(CC_IS_GCC),1)
ifneq (,$(filter x86_64%,$(shell $(CC) -dumpmachine)))
HARDEN_CFLAGS += -fcf-protection=full
endif
endif

CFLAGS   = $(HARDEN_CFLAGS)
# ML-specific flags: -O3, fma contraction, auto-vectorization
ML_CFLAGS = -O3 -ffp-contract=fast -funroll-loops -ftree-vectorize
LDFLAGS  = $(HARDEN_LDFLAGS) -lm -lpthread
ARFLAGS  = rcs

# All header directories live under libs/
LIBDIRS  := $(shell find ./libs -maxdepth 4 -type d -not -path '*/.git/*' | sort -u)

# Sequential library sources (all scl_*.c under libs/, excluding test files)
LIBSRCS  := $(shell find ./libs -name 'scl_*.c' -not -name 'test_*.c' -not -path '*/.git/*' -not -path '*/ml/*simd_avx*' -not -path '*/ml/*simd_neon*')
LIBOBJS  := $(patsubst %.c, build/%.o, $(LIBSRCS))
LIBNAME  = libscl.a

# ML SIMD target-specific sources (compiled with extra ISA flags but not by default)
ML_SIMD_SRCS := $(shell find ./libs/ml -name 'scl_ml_simd_avx2.c' -not -path '*/.git/*' 2>/dev/null)
ML_SIMD_SRCS += $(shell find ./libs/ml -name 'scl_ml_simd_avx512.c' -not -path '*/.git/*' 2>/dev/null)
ML_SIMD_SRCS += $(shell find ./libs/ml -name 'scl_ml_simd_neon.c' -not -path '*/.git/*' 2>/dev/null)

# Concurrent library sources (all scl_concurrent_*.c under libs/)
CONC_SRCS := $(shell find ./libs -name 'scl_concurrent_*.c' -not -path '*/.git/*')
CONC_OBJS := $(patsubst %.c, build/%.o, $(CONC_SRCS))
CONC_LIBNAME = libscl_concurrent.a

# Test binaries (live in ./tests/)
TESTSRCS := $(shell find ./tests -name 'test_*.c' -not -path '*/.git/*')
TESTBINS := $(patsubst %.c, build/%_bin, $(TESTSRCS))

# -I flags for every directory under libs/
INCFLAGS := $(addprefix -I, $(LIBDIRS))

# Fuzz targets (live in ./tests/fuzz/)
FUZZSRCS := $(wildcard tests/fuzz/*.c)
FUZZBINS := $(patsubst %.c, build/%_fuzzbin, $(FUZZSRCS))

.PHONY: all lib test clean check asan tsan ubsan msan fuzz

all: lib test

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

$(LIBNAME): $(LIBOBJS)
	$(AR) $(ARFLAGS) $@ $^

$(CONC_LIBNAME): $(CONC_OBJS)
	$(AR) $(ARFLAGS) $@ $^

lib: $(LIBNAME) $(CONC_LIBNAME)

build/%_bin: %.c $(LIBNAME)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wno-unused-result $(INCFLAGS) -o $@ $< -L. -lscl $(LDFLAGS)

test: $(TESTBINS)
	@passed=0; failed=0; \
	for t in $(TESTBINS); do \
		name=$$(basename $$t _bin); \
		printf "%-50s" "$$name"; \
		if ./$$t > /tmp/$$$$.log 2>&1; then \
			echo "✓ PASS"; passed=$$((passed+1)); \
		else \
			echo "✗ FAIL"; tail -5 /tmp/$$$$.log; failed=$$((failed+1)); \
		fi; \
	done; \
	echo ""; echo "=== $$((passed+failed)) tests: $$passed passed, $$failed failed ==="; \
	[ $$failed -eq 0 ]

# ── Sanitizer targets ─────────────────────────────────────────
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

# ── Fuzz targets (requires libFuzzer / -fsanitize=fuzzer) ────
build/%_fuzzbin: %.c $(LIBNAME)
	@mkdir -p $(dir $@)
	$(CC) $(HARDEN_CFLAGS) -g -fsanitize=fuzzer,address $(INCFLAGS) -o $@ $< -L. -lscl $(HARDEN_LDFLAGS) -lm -lpthread

fuzz: $(FUZZBINS)

check: asan ubsan

clean:
	rm -f $(LIBNAME) $(CONC_LIBNAME)
	rm -rf build 2>/dev/null; true
