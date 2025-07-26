# JSON Diff C Library

A C11 implementation of JSON diffing and patching functionality, following the Linux Kernel Style Guide.

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
struct json_value *json_diff(const struct json_value *left,
                             const struct json_value *right,
                             const struct json_diff_options *opts);

/**
 * json_patch - Apply a diff to a JSON value
 * @original: original JSON value
 * @diff: diff to apply
 *
 * Return: patched JSON value or NULL on failure
 */
struct json_value *json_patch(const struct json_value *original,
                              const struct json_value *diff);
```

### Example Usage

```c
#include "json_diff.h"

int main(void)
{
    // Create first object: {"test": 1}
    struct json_value *obj1 = json_value_create_object();
    struct json_value *val1 = json_value_create_number(1);
    json_object_set(obj1->data.object_val, "test", val1);

    // Create second object: {"test": 2}  
    struct json_value *obj2 = json_value_create_object();
    struct json_value *val2 = json_value_create_number(2);
    json_object_set(obj2->data.object_val, "test", val2);

    // Create diff
    struct json_value *diff = json_diff(obj1, obj2, NULL);
    
    // Apply patch
    struct json_value *patched = json_patch(obj1, diff);
    
    // Verify result equals obj2
    assert(json_value_equal(patched, obj2, true));
    
    // Cleanup
    json_value_free(obj1);
    json_value_free(obj2);
    json_value_free(diff);
    json_value_free(patched);
    json_value_free(val1);
    json_value_free(val2);
    
    return 0;
}
```

## Building

```bash
make all          # Build library and tests
make test         # Run tests
make clean        # Clean build artifacts
make install      # Install to system (requires root)
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
make test
```

## License

SPDX-License-Identifier: Apache-2.0
