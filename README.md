# JSON Diff C Library

A C11 implementation of JSON diffing and patching functionality, following the Linux Kernel Style Guide.

This is just experimental and hasn't been optimised at all.

## Features

- **Diff**: Create diffs between two JSON structures
- **Patch**: Apply diffs to JSON structures  
- **Array Support**: Handle array insertions, deletions, and modifications
- **Object Support**: Handle object key additions, deletions, and changes
- **Strict Equality**: Optional strict vs loose equality comparison for numbers
- **Memory Safe**: Proper memory management with cleanup functions

## API Documentation

### Core Functions

```c
/**
 * json_diff - Create a diff between two JSON values
 * @left: first JSON value
 * @right: second JSON value  
 * @opts: diff options (can be NULL for defaults)
 *
 * Return: diff object or NULL if values are equal
 */
cJSON *json_diff(const cJSON *left, const cJSON *right,
                 const struct json_diff_options *opts);

/**
 * json_patch - Apply a diff to a JSON value
 * @original: original JSON value
 * @diff: diff to apply
 *
 * Return: patched JSON value or NULL on failure
 */
cJSON *json_patch(const cJSON *original, const cJSON *diff);

/**
 * json_value_equal - Compare two JSON values for equality
 * @left: first value
 * @right: second value
 * @strict: use strict equality for numbers
 *
 * Return: true if equal, false otherwise
 */
bool json_value_equal(const cJSON *left, const cJSON *right, bool strict);
```

### Example Usage

```c
#include "json_diff.h"

int main(void)
{
    // Create first object: {"test": 1}
    cJSON *obj1 = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj1, "test", 1);

    // Create second object: {"test": 2}  
    cJSON *obj2 = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj2, "test", 2);

    // Create diff
    cJSON *diff = json_diff(obj1, obj2, NULL);
    
    // Apply patch
    cJSON *patched = json_patch(obj1, diff);
    
    // Verify result equals obj2
    assert(json_value_equal(patched, obj2, true));
    
    // Cleanup
    cJSON_Delete(obj1);
    cJSON_Delete(obj2);
    cJSON_Delete(diff);
    cJSON_Delete(patched);
    
    return 0;
}
```

## Dependencies

This library requires the cJSON library:

```bash
# Ubuntu/Debian
sudo apt-get install libcjson-dev

# CentOS/RHEL/Fedora
sudo yum install cjson-devel
# or
sudo dnf install cjson-devel

# macOS with Homebrew
brew install cjson
```

You also need Meson build system:

```bash
# Ubuntu/Debian
sudo apt-get install meson

# CentOS/RHEL/Fedora
sudo dnf install meson

# macOS with Homebrew
brew install meson

# Or via pip
pip install meson
```

## Building

```bash
meson setup builddir       # Configure build
meson compile -C builddir  # Build library and tests
meson test -C builddir     # Run tests
meson install -C builddir  # Install to system (requires root)
```

### Alternative build commands

```bash
# Quick build and test
meson setup builddir && meson compile -C builddir && meson test -C builddir

# Clean build
rm -rf builddir
meson setup builddir
```

## Diff Format

The library uses a diff format compatible with jsondiffpatch:

- **Simple changes**: `[old_value, new_value]`
- **Additions**: `[new_value]`  
- **Deletions**: `[old_value, 0, 0]`
- **Array changes**: Object with `_t: "a"` marker and indexed changes

### Examples

Simple value change:
```json
{"test": [1, 2]}
```

Array with deletion:
```json
{"test": {"_0": [1, 0, 0], "_t": "a"}}
```

## Testing

The test suite includes:

- Basic object diffing and patching
- Array diffing with insertions/deletions
- Strict vs non-strict equality comparison
- Memory leak detection
- Edge cases and error conditions

Run tests with:
```bash
meson test -C builddir
```

### Performance Profiling

The project includes performance profiling tests based on the Scala implementation:

```bash
# Run performance profile tests
meson compile -C builddir profile

# The profile tests require JSON data files in profile-data/ directory:
# - profile-data/cdc.json
# - profile-data/edg.json  
# - profile-data/ModernAtomic.json
# - profile-data/LegacyAtomic.json
```

### Medium diff micro‑benchmark

To isolate just the JSON diff performance for the medium dataset, run:
```bash
# via Meson
meson compile -C builddir bench-medium

# via Makefile (after make)
make bench
```

### Parser micro‑benchmark

To measure raw JSON parsing cost for the medium dataset:
```bash
# via Makefile
make bench-parse
n# via Meson
meson compile -C builddir bench_parse && ninja -C builddir bench-parse
```
This performs 5 warm‑up iterations then 50 timed calls to json_diff, printing the total and per‑iteration latency.

The profile tests measure:
- Medium-sized JSON diff performance (50 iterations)
- Large JSON diff performance (single iteration)
- Array diff and patch performance with 1000 elements

### Fuzzing

The project includes fuzzing support using libFuzzer to find bugs and edge cases:

```bash
# Build with fuzzing support (requires clang)
CC=clang meson setup builddir-fuzz
meson compile -C builddir-fuzz

# Create corpus directory for fuzzer inputs
mkdir -p corpus

# Add some initial test cases to corpus
echo '{"test": 1}' > corpus/simple1.json
echo '{"test": 2}' > corpus/simple2.json
echo '{"arr": [1,2,3]}' > corpus/array1.json
echo '{"arr": [2,3,4]}' > corpus/array2.json

# Run fuzzer for 60 seconds
meson compile -C builddir-fuzz fuzz

# Run fuzzer for 5 minutes (longer session)
meson compile -C builddir-fuzz fuzz-long

# Run fuzzer with custom arguments (runs indefinitely until stopped)
meson compile -C builddir-fuzz fuzz-custom
```

The fuzzer will automatically generate test cases and report any crashes or hangs.

## License

SPDX-License-Identifier: Apache-2.0
