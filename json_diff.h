/* SPDX-License-Identifier: Apache-2.0 */
#ifndef JSON_DIFF_H
#define JSON_DIFF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <cjson/cJSON.h>

/**
 * struct json_diff_options - Options for JSON diffing
 * @strict_equality: use strict equality comparison for numbers
 */
struct json_diff_options {
	bool strict_equality;
};

/* Function prototypes */
cJSON *json_diff(const cJSON *left, const cJSON *right,
		 const struct json_diff_options *opts);

cJSON *json_patch(const cJSON *original, const cJSON *diff);

bool json_value_equal(const cJSON *left, const cJSON *right, bool strict);

/* Utility functions for creating diff/patch structures */
cJSON *create_change_array(const cJSON *old_val, const cJSON *new_val);
cJSON *create_addition_array(const cJSON *new_val);
cJSON *create_deletion_array(const cJSON *old_val);

#endif /* JSON_DIFF_H */
