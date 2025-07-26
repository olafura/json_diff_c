# SPDX-License-Identifier: GPL-2.0

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -O2 -g
LDFLAGS = -lm

SOURCES = json_diff.c
HEADERS = json_diff.h
TEST_SOURCES = test_json_diff.c
OBJECTS = $(SOURCES:.c=.o)
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)

TARGET = libjsondiff.a
TEST_TARGET = test_json_diff

.PHONY: all clean test

all: $(TARGET) $(TEST_TARGET)

$(TARGET): $(OBJECTS)
	ar rcs $@ $^

$(TEST_TARGET): $(TEST_OBJECTS) $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(OBJECTS) $(TEST_OBJECTS) $(TARGET) $(TEST_TARGET)

install: $(TARGET)
	install -d /usr/local/include
	install -d /usr/local/lib
	install -m 644 $(HEADERS) /usr/local/include/
	install -m 644 $(TARGET) /usr/local/lib/

.SUFFIXES: .c .o
