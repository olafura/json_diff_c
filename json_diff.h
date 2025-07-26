/* SPDX-License-Identifier: GPL-2.0 */
#ifndef JSON_DIFF_H
#define JSON_DIFF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * enum json_type - JSON value types
 * @JSON_NULL: null value
 * @JSON_BOOL: boolean value
 * @JSON_NUMBER: numeric value
 * @JSON_STRING: string value
 * @JSON_ARRAY: array value
 * @JSON_OBJECT: object value
 */
enum json_type {
	JSON_NULL,
	JSON_BOOL,
	JSON_NUMBER,
	JSON_STRING,
	JSON_ARRAY,
	JSON_OBJECT
};

/**
 * struct json_value - JSON value representation
 * @type: type of the JSON value
 * @data: union containing the actual value
 * @data.bool_val: boolean value
 * @data.number_val: numeric value
 * @data.string_val: string value
 * @data.array_val: array value
 * @data.object_val: object value
 */
struct json_value {
	enum json_type type;
	union {
		bool bool_val;
		double number_val;
		char *string_val;
		struct json_array *array_val;
		struct json_object *object_val;
	} data;
};

/**
 * struct json_array - JSON array representation
 * @values: array of JSON values
 * @count: number of elements
 * @capacity: allocated capacity
 */
struct json_array {
	struct json_value *values;
	size_t count;
	size_t capacity;
};

/**
 * struct json_object_pair - JSON object key-value pair
 * @key: object key
 * @value: object value
 */
struct json_object_pair {
	char *key;
	struct json_value value;
};

/**
 * struct json_object - JSON object representation
 * @pairs: array of key-value pairs
 * @count: number of pairs
 * @capacity: allocated capacity
 */
struct json_object {
	struct json_object_pair *pairs;
	size_t count;
	size_t capacity;
};

/**
 * struct json_diff_options - Options for JSON diffing
 * @strict_equality: use strict equality comparison for numbers
 */
struct json_diff_options {
	bool strict_equality;
};

/* Function prototypes */
struct json_value *json_diff(const struct json_value *left,
			     const struct json_value *right,
			     const struct json_diff_options *opts);

struct json_value *json_patch(const struct json_value *original,
			      const struct json_value *diff);

void json_value_free(struct json_value *value);
struct json_value *json_value_clone(const struct json_value *value);
bool json_value_equal(const struct json_value *left,
		      const struct json_value *right,
		      bool strict);

/* Utility functions */
struct json_value *json_value_create_null(void);
struct json_value *json_value_create_bool(bool value);
struct json_value *json_value_create_number(double value);
struct json_value *json_value_create_string(const char *value);
struct json_value *json_value_create_array(void);
struct json_value *json_value_create_object(void);

int json_array_append(struct json_array *array, const struct json_value *value);
int json_object_set(struct json_object *object, const char *key,
		    const struct json_value *value);
struct json_value *json_object_get(const struct json_object *object,
				   const char *key);

#endif /* JSON_DIFF_H */
