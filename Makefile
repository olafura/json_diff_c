# SPDX-License-Identifier: Apache-2.0

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -O2 -g
LDFLAGS = -lm -lcjson

SOURCES = src/json_diff.c
HEADERS = src/json_diff.h
TEST_SOURCES = tests/test_json_diff.c
OBJECTS = $(SOURCES:.c=.o)
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)

TARGET = libjsondiff.a
TEST_TARGET = test_json_diff

.PHONY: all clean test meson-setup meson-compile meson-test meson-profile meson-clean

all: $(TARGET) $(TEST_TARGET)

# Meson build system targets
meson-setup:
	meson setup builddir

meson-compile: meson-setup
	meson compile -C builddir

meson-test: meson-compile
	meson test -C builddir

meson-profile: meson-compile
	meson compile -C builddir profile

meson-clean:
	rm -rf builddir

meson-install: meson-compile
	meson install -C builddir

$(TARGET): $(OBJECTS)
	ar rcs $@ $^

$(TEST_TARGET): $(TEST_OBJECTS) $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -Isrc -c $< -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(OBJECTS) $(TEST_OBJECTS) $(TARGET) $(TEST_TARGET)
	rm -rf builddir

install: $(TARGET)
	install -d /usr/local/include
	install -d /usr/local/lib
	install -m 644 $(HEADERS) /usr/local/include/
	install -m 644 $(TARGET) /usr/local/lib/

.SUFFIXES: .c .o
