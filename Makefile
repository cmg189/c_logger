# Makefile - c-logger, a small file-backed logging utility
#
#   make           build liblog.a
#   make example   build the example program
#   make run       build and run the example
#   make check     build and run the example under Address/UB sanitizers
#   make strict    build with -Werror
#   make clean     remove build artifacts

LIB     := liblog.a
EXAMPLE := example

LIB_SRCS := log.c
LIB_OBJS := $(LIB_SRCS:.c=.o)
DEPS     := $(LIB_SRCS:.c=.d) $(EXAMPLE).d

CC ?= gcc
AR ?= ar

STD      := -std=c11
# _POSIX_C_SOURCE exposes clock_gettime, localtime_r and pthreads while
# keeping -std=c11 strict rather than falling back to -std=gnu11.
FEATURES := -D_POSIX_C_SOURCE=200809L

WARNINGS := -Wall -Wextra \
            -Wshadow \
            -Wpointer-arith \
            -Wcast-qual \
            -Wstrict-prototypes \
            -Wmissing-prototypes \
            -Wwrite-strings \
            -Wformat=2 \
            -Wvla

# Set to -Werror to make warnings fatal:  make WERROR=-Werror
WERROR ?=

CFLAGS  ?= -O2 -g
CFLAGS  += $(STD) $(FEATURES) $(WARNINGS) $(WERROR) -I. -pthread -MMD -MP
LDFLAGS += -pthread

SANFLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer -g -O1

.PHONY: all run check strict clean help

all: $(LIB)

$(LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Named 'example', so 'make example' hits this rule directly.
$(EXAMPLE): example.c $(LIB_SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

run: $(EXAMPLE)
	./$(EXAMPLE)

# Built straight from source so it never shares objects with the normal build.
check: example.c $(LIB_SRCS)
	$(CC) $(STD) $(FEATURES) $(WARNINGS) $(SANFLAGS) -I. -pthread \
	      -o $(EXAMPLE)-asan $^
	ASAN_OPTIONS=detect_leaks=1 ./$(EXAMPLE)-asan

strict:
	$(MAKE) WERROR=-Werror example

clean:
	$(RM) $(LIB) $(LIB_OBJS) $(DEPS) \
	      $(EXAMPLE) $(EXAMPLE)-asan \
	      example.log

help:
	@echo "make          - build $(LIB)"
	@echo "make example  - build the example program"
	@echo "make run      - build and run the example"
	@echo "make check    - run the example under Address/UB sanitizers"
	@echo "make strict   - build with -Werror"
	@echo "make clean    - remove build artifacts"

-include $(DEPS)