/* SPDX-License-Identifier: Apache-2.0 */
#ifndef JSON_DIFF_H
#define JSON_DIFF_H

#include <stdbool.h>
#include <cjson/cJSON.h>

/**
 * struct json_diff_arena - Arena for diff allocations
 * @buf: internal buffer for allocations
 * @capacity: total buffer capacity in bytes
 * @offset: current used bytes (bump pointer)
 */
struct json_diff_arena {
    char *buf;
    size_t capacity;
    size_t offset;
};

/**
 * struct json_diff_options - Options for JSON diffing
 * @strict_equality: use strict equality comparison for numbers
 */
struct json_diff_options {
    /**
     * strict_equality - use strict equality comparison for numbers
     * arena: optional arena for diff allocations (NULL for heap alloc)
     */
	bool strict_equality;
	struct json_diff_arena *arena;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * json_diff_arena_init - Initialize a diff allocation arena
 * @arena: arena struct to initialize
 * @initial_capacity: initial buffer size in bytes
 */
void json_diff_arena_init(struct json_diff_arena *arena, size_t initial_capacity);

/**
 * json_diff_arena_cleanup - Free resources held by a diff arena
 * @arena: arena struct to cleanup
 */
void json_diff_arena_cleanup(struct json_diff_arena *arena);

/* Core API functions */

/**
 * json_diff - Create a diff between two cJSON values
 * @left: first JSON value (must not be NULL)
 * @right: second JSON value (must not be NULL)
 * @opts: diff options (can be NULL for defaults)
 *
 * Return: diff object or NULL if values are equal
 */
cJSON *json_diff(const cJSON *left, const cJSON *right,
		 const struct json_diff_options *opts);

/**
 * json_patch - Apply a diff to a cJSON value
 * @original: original JSON value (must not be NULL)
 * @diff: diff to apply (must not be NULL)
 *
 * Return: patched JSON value or NULL on failure
 */
cJSON *json_patch(const cJSON *original, const cJSON *diff);

/**
 * json_diff_str - Parse two JSON strings and diff them in one call
 * @left: NUL-terminated JSON text (first)
 * @right: NUL-terminated JSON text (second)
 * @opts: diff options (can be NULL for defaults)
 *
 * Return: diff object or NULL if values are equal or on error
 */
cJSON *json_diff_str(const char *left, const char *right,
                     const struct json_diff_options *opts);
/**
 * json_value_equal - Compare two cJSON values for equality
 * @left: first value (can be NULL)
 * @right: second value (can be NULL)
 * @strict: use strict equality for numbers
 *
 * Return: true if equal, false otherwise
 */
bool json_value_equal(const cJSON *left, const cJSON *right, bool strict);

/* Utility functions for creating diff/patch structures */

/**
 * create_change_array - Create a change array [old_value, new_value]
 * @old_val: old value (must not be NULL)
 * @new_val: new value (must not be NULL)
 *
 * Return: cJSON array or NULL on failure
 */
cJSON *create_change_array(const cJSON *old_val, const cJSON *new_val);

/**
 * create_addition_array - Create an addition array [new_value]
 * @new_val: new value (must not be NULL)
 *
 * Return: cJSON array or NULL on failure
 */
cJSON *create_addition_array(const cJSON *new_val);

/**
 * create_deletion_array - Create a deletion array [old_value, 0, 0]
 * @old_val: old value (must not be NULL)
 *
 * Return: cJSON array or NULL on failure
 */
cJSON *create_deletion_array(const cJSON *old_val);

#ifdef __cplusplus
}
#endif

#endif /* JSON_DIFF_H */
