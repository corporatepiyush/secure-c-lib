CC       = gcc
CFLAGS   = -std=c17 -Wall -Wextra -Wpedantic -Werror -O2
LDFLAGS  = -lm -lpthread
ARFLAGS  = rcs

COMMON   = common/scl_common.h

# Find all directories that contain scl_*.c files (library root dirs)
LIBDIRS  := $(shell find . -maxdepth 2 -type d -not -path '*/.git/*' -not -path './build/*' -not -name 'build' -not -name '.' -not -name '.git' | sort -u)

# Collect all library .c files (excluding test files)
LIBSRCS := $(shell find . -maxdepth 3 -name 'scl_*.c' -not -path '*/test_*' -not -path '*/.git/*')
LIBSRCS += $(shell find . -maxdepth 3 -name 'concurrent_*.c' -not -path '*/test_*' -not -path '*/.git/*')
LIBOBJS := $(patsubst ./%.c, build/%.o, $(LIBSRCS))
LIBNAME = libscl.a

# Collect all test files
TESTSRCS := $(shell find . -maxdepth 3 -name 'test_*.c' -not -path '*/.git/*')
TESTBINS := $(patsubst ./%.c, build/%_bin, $(TESTSRCS))

# Build -I flags for all library directories
INCFLAGS := $(addprefix -I, $(LIBDIRS)) -Icommon

.PHONY: all lib test clean

all: lib test

build/%.o: %.c $(COMMON)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCFLAGS) -I$(dir $<) -c $< -o $@

$(LIBNAME): $(LIBOBJS)
	$(AR) $(ARFLAGS) $@ $^

build/%_bin: %.c $(LIBNAME)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wno-unused-result $(INCFLAGS) -I$(dir $<) -o $@ $< -L. -lscl $(LDFLAGS)

lib: $(LIBNAME)

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
	rm -rf build $(LIBNAME)
