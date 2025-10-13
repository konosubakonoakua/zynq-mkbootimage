.PHONY: all clean distclean install uninstall help

CC=gcc
FMT=clang-format

MKBOOTIMAGE_NAME:=mkbootimage
EXBOOTIMAGE_NAME:=exbootimage
FPGAUTIL_NAME:=fpgautil

# Installation directories
PREFIX?=/usr/local
BINDIR?=$(PREFIX)/bin

VERSION_MAJOR:=2.3
VERSION_MINOR:=$(shell git rev-parse --short HEAD)
VERSION:=$(MKBOOTIMAGE_NAME) $(VERSION_MAJOR)-$(VERSION_MINOR)

COMMON_SRCS:=src/bif.c src/bootrom.c src/common.c \
             $(wildcard src/arch/*.c) $(wildcard src/file/*.c)
COMMON_HDRS:=src/bif.h src/bootrom.h src/common.h \
             $(wildcard src/arch/*.h) $(wildcard src/file/*.h)

MKBOOTIMAGE_SRCS:=$(COMMON_SRCS) src/mkbootimage.c
MKBOOTIMAGE_OBJS:=$(MKBOOTIMAGE_SRCS:.c=.o)

EXBOOTIMAGE_SRCS:=$(COMMON_SRCS) src/exbootimage.c
EXBOOTIMAGE_OBJS:=$(EXBOOTIMAGE_SRCS:.c=.o)

FPGAUTIL_SRCS:=src/fpgautil.c
FPGAUTIL_OBJS:=$(FPGAUTIL_SRCS:.c=.o)

ALL_SRCS:=$(COMMON_SRCS) src/mkbootimage.c src/exbootimage.c src/fpgautil.c
ALL_HDRS:=$(COMMON_HDRS)

INCLUDE_DIRS:=src

override CFLAGS += $(foreach includedir,$(INCLUDE_DIRS),-I$(includedir)) \
                   -DMKBOOTIMAGE_VER="\"$(VERSION)\"" \
                   -Wall -Wextra -Wpedantic \
                   --std=c11

FPGAUTIL_CFLAGS := -D_GNU_SOURCE
LDLIBS = -lelf

all: $(MKBOOTIMAGE_NAME) $(EXBOOTIMAGE_NAME) $(FPGAUTIL_NAME)

$(MKBOOTIMAGE_NAME): $(MKBOOTIMAGE_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(EXBOOTIMAGE_NAME): $(EXBOOTIMAGE_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(FPGAUTIL_NAME): $(FPGAUTIL_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

# Pattern rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Special rule for fpgautil with additional flags
src/fpgautil.o: src/fpgautil.c
	$(CC) $(CFLAGS) $(FPGAUTIL_CFLAGS) -c $< -o $@

format:
	$(FMT) -i $(ALL_SRCS) $(ALL_HDRS)

test:
	./tests/tester.sh

install: all
	install -d $(BINDIR)
	install -m 755 $(MKBOOTIMAGE_NAME) $(BINDIR)/$(MKBOOTIMAGE_NAME)
	install -m 755 $(EXBOOTIMAGE_NAME) $(BINDIR)/$(EXBOOTIMAGE_NAME)
	install -m 755 $(FPGAUTIL_NAME) $(BINDIR)/$(FPGAUTIL_NAME)
	@echo "Installed to $(BINDIR)"

uninstall:
	$(RM) $(BINDIR)/$(MKBOOTIMAGE_NAME)
	$(RM) $(BINDIR)/$(EXBOOTIMAGE_NAME)
	$(RM) $(BINDIR)/$(FPGAUTIL_NAME)
	@echo "Uninstalled from $(BINDIR)"

help:
	@echo "Available targets:"
	@echo "  all        - Build all executables (default target)"
	@echo "  clean      - Remove object files and executables"
	@echo "  distclean  - Same as clean"
	@echo "  install    - Install executables to $(BINDIR)"
	@echo "  uninstall  - Uninstall executables from $(BINDIR)"
	@echo "  help       - Show this help message"
	@echo "  format     - Format source code with clang-format"
	@echo "  test       - Run tests"
	@echo ""
	@echo "Variables:"
	@echo "  PREFIX     - Installation prefix (default: /usr/local)"
	@echo "  BINDIR     - Binary installation directory (default: \$$(PREFIX)/bin)"
	@echo ""
	@echo "Usage examples:"
	@echo "  make                           # Build all"
	@echo "  make install                   # Install to default location"
	@echo "  make install PREFIX=/usr       # Install to /usr/bin"
	@echo "  make uninstall                 # Uninstall from default location"
	@echo "  make clean                     # Clean build files"

clean:
	@- $(RM) $(MKBOOTIMAGE_NAME)
	@- $(RM) $(MKBOOTIMAGE_OBJS)
	@- $(RM) $(EXBOOTIMAGE_NAME)
	@- $(RM) $(EXBOOTIMAGE_OBJS)
	@- $(RM) $(FPGAUTIL_NAME)
	@- $(RM) $(FPGAUTIL_OBJS)

distclean: clean
