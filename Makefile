CC       = gcc
CFLAGS   = -std=c17 -Wall -Wextra -Wpedantic -Werror -O2
LDFLAGS  = -lm -lpthread
ARFLAGS  = rcs

# Find all directories that might contain headers (in structures/ and libs/)
LIBDIRS  := $(shell find ./structures -maxdepth 2 -type d -not -path '*/.git/*' | sort -u) \
            $(shell find ./libs -maxdepth 2 -type d -not -path '*/.git/*' | sort -u)

# Collect all sequential library .c files (excluding concurrent)
LIBSRCS := $(shell find ./structures ./libs -name 'scl_*.c' -not -path '*/.git/*')
LIBOBJS := $(patsubst %.c, build/%.o, $(LIBSRCS))
LIBNAME = libscl.a

# Collect all concurrent library .c files
CONC_SRCS := $(shell find ./structures ./libs -name 'concurrent_*.c' -not -path '*/.git/*')
CONC_OBJS := $(patsubst %.c, build/%.o, $(CONC_SRCS))
CONC_LIBNAME = libscl_concurrent.a

# Build -I flags for all library directories
INCFLAGS := $(addprefix -I, $(LIBDIRS))

.PHONY: all lib clean

all: lib

build/%.o: %.c libs/common/scl_common.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCFLAGS) -I$(dir $<) -c $< -o $@

$(LIBNAME): $(LIBOBJS)
	$(AR) $(ARFLAGS) $@ $^

$(CONC_LIBNAME): $(CONC_OBJS)
	$(AR) $(ARFLAGS) $@ $^

lib: $(LIBNAME) $(CONC_LIBNAME)

clean:
	rm -rf build $(LIBNAME) $(CONC_LIBNAME)
