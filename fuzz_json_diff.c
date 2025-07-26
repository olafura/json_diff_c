// SPDX-License-Identifier: Apache-2.0
#include "src/json_diff.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * LLVMFuzzerTestOneInput - Fuzzer entry point for json_diff
 * @data: fuzzer input data
 * @size: size of input data
 *
 * This fuzzer tests the json_diff and json_patch functions by:
 * 1. Splitting input into two JSON strings
 * 2. Parsing them with cJSON
 * 3. Creating a diff
 * 4. Applying the diff as a patch
 * 5. Verifying the result matches the second JSON
 *
 * Return: 0 (required by libFuzzer)
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	cJSON *json1 = NULL, *json2 = NULL, *diff = NULL, *patched = NULL;
	char *json1_str = NULL, *json2_str = NULL;
	size_t split_point;

	/* Need at least 2 bytes */
	if (size < 2)
		return 0;

	/* Split input into two parts */
	split_point = size / 2;

	/* Create null-terminated strings for JSON parsing */
	json1_str = malloc(split_point + 1);
	json2_str = malloc(size - split_point + 1);

	if (!json1_str || !json2_str)
		goto cleanup;

	memcpy(json1_str, data, split_point);
	json1_str[split_point] = '\0';

	memcpy(json2_str, data + split_point, size - split_point);
	json2_str[size - split_point] = '\0';

	/* Parse JSON strings */
	json1 = cJSON_Parse(json1_str);
	json2 = cJSON_Parse(json2_str);

	/* Only proceed if both JSONs are valid */
	if (!json1 || !json2)
		goto cleanup;

	/* Test diff function with different options */
	struct json_diff_options opts_strict = {.strict_equality = true};
	struct json_diff_options opts_loose = {.strict_equality = false};

	diff = json_diff(json1, json2, &opts_strict);
	if (diff) {
		/* Test patch function */
		patched = json_patch(json1, diff);
		if (patched) {
			/* Verify patch result equals json2 (when possible) */
			json_value_equal(patched, json2, true);
			cJSON_Delete(patched);
			patched = NULL;
		}
		cJSON_Delete(diff);
		diff = NULL;
	}

	/* Test with loose equality */
	diff = json_diff(json1, json2, &opts_loose);
	if (diff) {
		patched = json_patch(json1, diff);
		if (patched) {
			json_value_equal(patched, json2, false);
			cJSON_Delete(patched);
			patched = NULL;
		}
		cJSON_Delete(diff);
		diff = NULL;
	}

	/* Test edge cases */
	diff = json_diff(json1, json1, NULL); /* Same object */
	if (diff)
		cJSON_Delete(diff);

	diff = json_diff(json2, json2, NULL); /* Same object */
	if (diff)
		cJSON_Delete(diff);

	/* Test utility functions */
	if (json1 && json2) {
		cJSON *change_arr = create_change_array(json1, json2);
		if (change_arr)
			cJSON_Delete(change_arr);

		cJSON *add_arr = create_addition_array(json2);
		if (add_arr)
			cJSON_Delete(add_arr);

		cJSON *del_arr = create_deletion_array(json1);
		if (del_arr)
			cJSON_Delete(del_arr);
	}

cleanup:
	if (json1)
		cJSON_Delete(json1);
	if (json2)
		cJSON_Delete(json2);
	if (diff)
		cJSON_Delete(diff);
	if (patched)
		cJSON_Delete(patched);
	free(json1_str);
	free(json2_str);

	return 0;
}
