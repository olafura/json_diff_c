# SPDX-License-Identifier: Apache-2.0

BUILD_DIR := builddir

.PHONY: all setup build test bench bench-medium bench-pipeline bench-parse bench-jsmn profile \
        install clean fuzz fuzz-long fuzz-custom clang-tidy clang-tidy-fix analyze format format-check

all: build

setup:
	meson setup $(BUILD_DIR)

build: setup
	meson compile -C $(BUILD_DIR)

test: build
	meson test -C $(BUILD_DIR)

bench: bench-medium

bench-medium: build
	meson run bench-medium -C $(BUILD_DIR)

bench-pipeline: build
	meson run bench-pipeline -C $(BUILD_DIR)

bench-parse: build
	meson compile -C $(BUILD_DIR) bench_parse
	$(BUILD_DIR)/bench_parse

profile: build
	meson run profile -C $(BUILD_DIR)

install: build
	meson install -C $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

fuzz: build
	meson run fuzz -C $(BUILD_DIR)

fuzz-long: build
	meson run fuzz-long -C $(BUILD_DIR)

fuzz-custom: build
	meson run fuzz-custom -C $(BUILD_DIR)

clang-tidy: build
	meson run clang-tidy -C $(BUILD_DIR)

clang-tidy-fix: build
	meson run clang-tidy-fix -C $(BUILD_DIR)

analyze: build
	meson run analyze -C $(BUILD_DIR)

format:
	rg -l --glob '*.c' --glob '*.h' --null . \
		| xargs -0 clang-format -i

format-check:
	rg -l --glob '*.c' --glob '*.h' --null . \
		| xargs -0 clang-format --dry-run --Werror
