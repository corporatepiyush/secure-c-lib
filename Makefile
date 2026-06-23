MAKEFLAGS += -j$(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
CC       = gcc
CFLAGS   = -std=c17 -Wall -Wextra -Wpedantic -Werror -O2 \
           -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE \
           -Wformat -Wformat-security -Wnull-dereference
LDFLAGS  = -lm -lpthread
ARFLAGS  = rcs

# All header directories live under libs/
LIBDIRS  := $(shell find ./libs -maxdepth 4 -type d -not -path '*/.git/*' | sort -u)

# Sequential library sources (all scl_*.c under libs/, excluding test files)
LIBSRCS  := $(shell find ./libs -name 'scl_*.c' -not -name 'test_*.c' -not -path '*/.git/*')
LIBOBJS  := $(patsubst %.c, build/%.o, $(LIBSRCS))
LIBNAME  = libscl.a

# Concurrent library sources (all scl_concurrent_*.c under libs/)
CONC_SRCS := $(shell find ./libs -name 'scl_concurrent_*.c' -not -path '*/.git/*')
CONC_OBJS := $(patsubst %.c, build/%.o, $(CONC_SRCS))
CONC_LIBNAME = libscl_concurrent.a

# Test binaries (live in ./tests/)
TESTSRCS := $(shell find ./tests -name 'test_*.c' -not -path '*/.git/*')
TESTBINS := $(patsubst %.c, build/%_bin, $(TESTSRCS))

# -I flags for every directory under libs/
INCFLAGS := $(addprefix -I, $(LIBDIRS))

.PHONY: all lib test clean

all: lib test

build/%.o: %.c libs/common/scl_common.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCFLAGS) -I$(dir $<) -c $< -o $@

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

clean:
	rm -rf build $(LIBNAME) $(CONC_LIBNAME)
