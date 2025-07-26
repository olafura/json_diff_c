# SPDX-License-Identifier: Apache-2.0

BUILD_DIR := builddir

.PHONY: all setup build test bench bench-medium bench-pipeline bench-parse bench-jsmn profile \
        install clean fuzz fuzz-long fuzz-custom tidy tidy-fix format format-check \
        advanced-test gen-test-quick gen-test gen-test-extensive prop-test

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
	meson compile -C $(BUILD_DIR) profile

install: build
	meson install -C $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

fuzz: build
	cd $(BUILD_DIR) && meson run fuzz

fuzz-long: build
	cd $(BUILD_DIR) && meson run fuzz-long

fuzz-custom: build
	cd $(BUILD_DIR) && meson run fuzz-custom

tidy:
	rg -l --glob '*.c' --glob '*.h' --null src \
		| xargs -0 clang-tidy -p $(BUILD_DIR)

tidy-fix:
	rg -l --glob '*.c' --glob '*.h' --null src \
		| xargs -0 clang-tidy --format -p $(BUILD_DIR)

format:
	rg -l --glob '*.c' --glob '*.h' --null . \
		| xargs -0 clang-format -i

format-check:
	rg -l --glob '*.c' --glob '*.h' --null . \
		| xargs -0 clang-format --dry-run --Werror

# Advanced testing targets
advanced-test: build
	@echo "Running advanced testing suite..."
	@./run_advanced_tests.sh

gen-test-quick: build
	@echo "Running quick generative tests..."
	@./$(BUILD_DIR)/test_generative --tests 100

gen-test: build
	@echo "Running standard generative tests..."
	@./$(BUILD_DIR)/test_generative --tests 1000

gen-test-extensive: build
	@echo "Running extensive generative tests..."
	@./$(BUILD_DIR)/test_generative --tests 5000

prop-test: build
	@echo "Running property tests..."
	@./$(BUILD_DIR)/test_properties
