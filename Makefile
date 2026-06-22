CC       = gcc
CFLAGS   = -std=c17 -Wall -Wextra -Wpedantic -Werror -O2
LDFLAGS  = -lm -lpthread
ARFLAGS  = rcs

# Find all directories that might contain headers
LIBDIRS  := $(shell find . -maxdepth 4 -type d -not -path '*/.git/*' -not -path './build/*' -not -name 'build' -not -name '.' -not -name '.git' | sort -u)

# Collect all sequential library .c files (excluding test and concurrent)
LIBSRCS := $(shell find . -name 'scl_*.c' -not -path '*/test_*' -not -path '*/.git/*')
LIBOBJS := $(patsubst ./%.c, build/%.o, $(LIBSRCS))
LIBNAME = libscl.a

# Collect all concurrent library .c files (excluding test files)
CONC_SRCS := $(shell find . -name 'concurrent_*.c' -not -path '*/test_*' -not -path '*/.git/*')
CONC_OBJS := $(patsubst ./%.c, build/%.o, $(CONC_SRCS))
CONC_LIBNAME = libscl_concurrent.a

# Collect all test files
TESTSRCS := $(shell find . -name 'test_*.c' -not -path '*/.git/*')
TESTBINS := $(patsubst ./%.c, build/%_bin, $(TESTSRCS))
CONC_TESTSRCS := $(shell find . -name 'test_concurrent_*.c' -not -path '*/.git/*')
CONC_TESTBINS := $(patsubst ./%.c, build/%_bin, $(CONC_TESTSRCS))
SEQ_TESTSRCS := $(filter-out $(CONC_TESTSRCS), $(TESTSRCS))
SEQ_TESTBINS := $(patsubst ./%.c, build/%_bin, $(SEQ_TESTSRCS))

# Build -I flags for all library directories
INCFLAGS := $(addprefix -I, $(LIBDIRS))

.PHONY: all lib test clean

all: lib test

build/%.o: %.c common/scl_common.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCFLAGS) -I$(dir $<) -c $< -o $@

$(LIBNAME): $(LIBOBJS)
	$(AR) $(ARFLAGS) $@ $^

$(CONC_LIBNAME): $(CONC_OBJS)
	$(AR) $(ARFLAGS) $@ $^

build/%_bin: %.c $(LIBNAME)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wno-unused-result $(INCFLAGS) -I$(dir $<) -o $@ $< -L. -lscl $(LDFLAGS)

# Concurrent test binaries link both libraries
$(CONC_TESTBINS): build/%_bin: %.c $(LIBNAME) $(CONC_LIBNAME)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wno-unused-result $(INCFLAGS) -I$(dir $<) -o $@ $< -L. -lscl_concurrent -lscl $(LDFLAGS)

lib: $(LIBNAME) $(CONC_LIBNAME)

test: $(TESTBINS)
	@passed=0; failed=0; \
	for t in $(TESTBINS); do \
		name=$$(echo $$t | sed 's|build/||; s|_bin$$||'); \
		printf "%-55s" "$$name"; \
		if ./$$t > /tmp/$$$$.log 2>&1; then \
			echo "PASS"; passed=$$((passed+1)); \
		else \
			echo "FAIL"; tail -3 /tmp/$$$$.log; failed=$$((failed+1)); \
		fi; \
	done; \
	echo "=== $$((passed+failed)) tests: $$passed passed, $$failed failed ==="; \
	[ $$failed -eq 0 ]

clean:
	rm -rf build $(LIBNAME) $(CONC_LIBNAME)
