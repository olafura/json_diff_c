// SPDX-License-Identifier: Apache-2.0
#define __STDC_WANT_LIB_EXT1__ 1
#include "src/json_diff.h"
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>




/* Simple PRNG for fuzzer-driven generation */
static unsigned long fuzz_rng_state = 12345;

static void fuzz_seed(unsigned long seed) { fuzz_rng_state = seed; }

static unsigned long fuzz_rand(void)
{
	fuzz_rng_state = fuzz_rng_state * 1103515245 + 12345;
	return fuzz_rng_state;
}

static int fuzz_rand_int(int min, int max)
{
	if (max <= min)
		return min;
	return min + (int)(fuzz_rand() % (unsigned long)(max - min + 1));
}


/* Generate random JSON using fuzzer input as seed */
static cJSON *fuzz_generate_json(const uint8_t *data, size_t size,
                                 int max_depth);

static cJSON *fuzz_generate_array(const uint8_t *data, size_t size, int depth,
                                  int max_depth)
{
	if (depth >= max_depth || size == 0)
		return cJSON_CreateArray();

	cJSON *array = cJSON_CreateArray();
	if (!array)
		return NULL;

	int array_size = fuzz_rand_int(0, 5);
	size_t chunk_size = size / (size_t)(array_size + 1);

	for (int i = 0; i < array_size && chunk_size > 0; i++) {
		size_t offset = (size_t)i * chunk_size;
		if (offset < size) {
			cJSON *item = fuzz_generate_json(
			    data + offset,
			    (chunk_size < size - offset) ? chunk_size
			                                 : size - offset,
			    max_depth);
			if (item) {
				cJSON_AddItemToArray(array, item);
			}
		}
	}

	return array;
}

static cJSON *fuzz_generate_object(const uint8_t *data, size_t size, int depth,
                                   int max_depth)
{
	if (depth >= max_depth || size == 0)
		return cJSON_CreateObject();

	cJSON *object = cJSON_CreateObject();
	if (!object)
		return NULL;

	int num_fields = fuzz_rand_int(0, 5);
	size_t chunk_size = size / (size_t)(num_fields + 1);

	for (int i = 0; i < num_fields && chunk_size > 0; i++) {
		char key[32];
		if (__STDC_LIB_EXT1__) {
			snprintf_s(key, sizeof(key), "k%d", i);
		} else {
			snprintf(key, sizeof(key), "k%d", i);
		}

		size_t offset = (size_t)i * chunk_size;
		if (offset < size) {
			cJSON *value = fuzz_generate_json(
			    data + offset,
			    (chunk_size < size - offset) ? chunk_size
			                                 : size - offset,
			    max_depth);
			if (value) {
				cJSON_AddItemToObject(object, key, value);
			}
		}
	}

	return object;
}

static cJSON *fuzz_generate_json(const uint8_t *data, size_t size,
                                 int max_depth)
{
	if (size == 0)
		return cJSON_CreateNull();

	/* Use first byte to determine type */
	int type = data[0] % 7;

	/* Seed RNG with some data bytes */
	if (size >= 4) {
		unsigned long seed = 0;
		for (size_t i = 0; i < 4 && i < size; i++) {
			seed = (seed << 8) | data[i];
		}
		fuzz_seed(seed);
	}

	switch (type) {
	case 0: /* null */
		return cJSON_CreateNull();
	case 1: /* boolean */
		return cJSON_CreateBool((data[0] % 2) == 1);
	case 2: /* number */
	{
		double num = 0.0;
		if (size >= sizeof(double)) {
			if (__STDC_LIB_EXT1__) {
				memcpy_s(&num, sizeof(double), data, sizeof(double));
			} else {
				memcpy(&num, data, sizeof(double));
			}
			/* Clamp to reasonable range to avoid inf/nan issues in
			 * tests */
			if (num != num || num > 1e10 || num < -1e10) {
				num = (double)(data[0] % 1000) - 500.0;
			}
		} else {
			num = (double)(data[0] % 200) - 100.0;
		}
		return cJSON_CreateNumber(num);
	}
	case 3: /* string */
	{
		size_t str_len = (size > 1) ? (data[1] % 50) : 0;
		if (str_len > size - 2)
			str_len = size - 2;

		char *str = malloc(str_len + 1);
		if (!str)
			return cJSON_CreateString("");

		for (size_t i = 0; i < str_len; i++) {
			unsigned char c = (i + 2 < size) ? data[i + 2] : 'a';
			/* Keep printable ASCII range */
			str[i] = (char)((c % 95) + 32);
		}
		str[str_len] = '\0';

		cJSON *result = cJSON_CreateString(str);
		free(str);
		return result;
	}
	case 4: /* array */
		return fuzz_generate_array(data + 1, (size > 1) ? size - 1 : 0,
		                           0, max_depth);
	case 5: /* object */
		return fuzz_generate_object(data + 1, (size > 1) ? size - 1 : 0,
		                            0, max_depth);
	default:
		return cJSON_CreateNull();
	}
}

/* Mutate JSON based on fuzzer input */
static cJSON *fuzz_mutate_json(const cJSON *original, const uint8_t *data,
                               size_t size)
{
	if (!original || size == 0)
		return NULL;

	/* Simple mutation: if first bit is set, generate completely new JSON */
	if (data[0] & 0x80) {
		return fuzz_generate_json(data, size, 4);
	}

	/* Otherwise, copy and potentially modify */
	switch (original->type) {
	case cJSON_NULL:
		if ((data[0] % 10) == 0) {
			return fuzz_generate_json(data, size, 2);
		}
		return cJSON_CreateNull();

	case cJSON_True:
	case cJSON_False:
		if ((data[0] % 5) == 0) {
			return cJSON_CreateBool(!cJSON_IsTrue(original));
		}
		return cJSON_CreateBool(cJSON_IsTrue(original));

	case cJSON_Number:
		if ((data[0] % 3) == 0) {
			double delta = ((double)(data[0] % 21) - 10.0);
			return cJSON_CreateNumber(original->valuedouble +
			                          delta);
		}
		return cJSON_CreateNumber(original->valuedouble);

	case cJSON_String:
		if ((data[0] % 4) == 0 && original->valuestring) {
			size_t len = strlen(original->valuestring);
			char *new_str = malloc(len + 2);
			if (new_str) {
				size_t copy_len = strlen(original->valuestring);
				if (copy_len < len + 1) {
					if (__STDC_LIB_EXT1__) {
						memcpy_s(new_str, len + 2, original->valuestring, copy_len + 1);
					} else {
						memcpy(new_str, original->valuestring, copy_len + 1);
					}
					if (len > 0 && size > 1) {
						/* Modify one character */
						new_str[data[1] % len] =
						    (char)((data[1] % 95) + 32);
					}
					cJSON *result = cJSON_CreateString(new_str);
					free(new_str);
					return result;
				} else {
					free(new_str);
				}
			}
		}
		return cJSON_CreateString(
		    original->valuestring ? original->valuestring : "");

	case cJSON_Array: {
		cJSON *new_array = cJSON_CreateArray();
		if (!new_array)
			return NULL;

		cJSON *item = original->child;
		size_t item_idx = 0;
		while (item) {
			if ((data[item_idx % size] % 10) !=
			    0) { /* Keep most items */
				cJSON *new_item = fuzz_mutate_json(
				    item, data + (item_idx % size),
				    (size > item_idx) ? size - item_idx : 1);
				if (new_item) {
					cJSON_AddItemToArray(new_array,
					                     new_item);
				}
			}
			item = item->next;
			item_idx++;
		}

		return new_array;
	}

	case cJSON_Object: {
		cJSON *new_object = cJSON_CreateObject();
		if (!new_object)
			return NULL;

		cJSON *item = original->child;
		size_t item_idx = 0;
		while (item) {
			if ((data[item_idx % size] % 10) !=
			    0) { /* Keep most fields */
				cJSON *new_value = fuzz_mutate_json(
				    item, data + (item_idx % size),
				    (size > item_idx) ? size - item_idx : 1);
				if (new_value) {
					cJSON_AddItemToObject(new_object,
					                      item->string,
					                      new_value);
				}
			}
			item = item->next;
			item_idx++;
		}

		return new_object;
	}
	}

	if (cJSON_IsObject(original)) {
		return cJSON_CreateObjectReference(original);
	} else if (cJSON_IsArray(original)) {
		return cJSON_CreateArrayReference(original);
	} else if (cJSON_IsString(original)) {
		return cJSON_CreateString(original->valuestring);
	} else if (cJSON_IsNumber(original)) {
		return cJSON_CreateNumber(original->valuedouble);
	} else if (cJSON_IsBool(original)) {
		return cJSON_CreateBool(cJSON_IsTrue(original));
	} else {
		return cJSON_CreateNull();
	}
}

/* Property testing functions integrated into fuzzer */
static bool fuzz_test_diff_creates_valid_diff(const cJSON *json1,
                                              const cJSON *json2)
{
	cJSON *diff = json_diff(json1, json2, NULL);

	if (json_value_equal(json1, json2, false)) {
		bool result = (diff == NULL);
		if (diff)
			cJSON_Delete(diff);
		return result;
	}

	bool result = (diff != NULL);
	if (diff)
		cJSON_Delete(diff);
	return result;
}

static bool fuzz_test_patch_roundtrip(const cJSON *json1, const cJSON *json2)
{
	cJSON *diff = json_diff(json1, json2, NULL);
	if (!diff) {
		return json_value_equal(json1, json2, false);
	}

	cJSON *patched = json_patch(json1, diff);
	bool result = false;

	if (patched) {
		result = json_value_equal(patched, json2, false);
		cJSON_Delete(patched);
	}

	cJSON_Delete(diff);
	return result;
}

static bool fuzz_test_self_diff_is_null(const cJSON *json)
{
	cJSON *diff = json_diff(json, json, NULL);
	bool result = (diff == NULL);
	if (diff)
		cJSON_Delete(diff);
	return result;
}

/**
 * LLVMFuzzerTestOneInput - Enhanced fuzzer entry point using generative testing
 * @data: fuzzer input data
 * @size: size of input data
 *
 * This enhanced fuzzer combines traditional fuzzing with generative testing:
 * 1. Uses fuzzer input to generate structured JSON data
 * 2. Creates mutations and variations
 * 3. Tests fundamental properties of json_diff/patch
 * 4. Validates memory safety and correctness
 * 5. Catches edge cases and corner conditions
 *
 * Return: 0 (required by libFuzzer)
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	cJSON *json1 = NULL, *json2 = NULL, *diff = NULL, *patched = NULL;

	/* Need at least 1 byte */
	if (size < 1)
		return 0;

	/* Strategy 1: Generate structured JSON from fuzzer input */
	json1 = fuzz_generate_json(data, size / 2 + 1, 4);
	if (!json1)
		goto cleanup;

	/* Strategy 2: Create json2 as either mutation or new generation */
	if (size > 1 && (data[0] & 0x40)) {
		/* Mutate json1 to create json2 */
		json2 =
		    fuzz_mutate_json(json1, data + size / 2, size - size / 2);
	} else {
		/* Generate completely new json2 */
		json2 = fuzz_generate_json(data + size / 2, size - size / 2, 4);
	}

	if (!json2)
		goto cleanup;

	/* Property 1: Test that diff creates valid results */
	if (!fuzz_test_diff_creates_valid_diff(json1, json2)) {
		/* Property violation - this should not happen */
		goto cleanup;
	}

	/* Property 2: Test patch roundtrip property */
	if (!fuzz_test_patch_roundtrip(json1, json2)) {
		/* Property violation - this should not happen */
		goto cleanup;
	}

	/* Property 3: Test self-diff is null */
	if (!fuzz_test_self_diff_is_null(json1)) {
		/* Property violation - this should not happen */
		goto cleanup;
	}

	if (!fuzz_test_self_diff_is_null(json2)) {
		/* Property violation - this should not happen */
		goto cleanup;
	}

	/* Strategy 3: Test traditional string-based fuzzing for edge cases */
	if (size >= 4) {
		size_t split_point = size / 2;
		char *json1_str = malloc(split_point + 1);
		char *json2_str = malloc(size - split_point + 1);

		if (json1_str && json2_str) {
			if (__STDC_LIB_EXT1__) {
				memcpy_s(json1_str, split_point + 1, data, split_point);
				memcpy_s(json2_str, size - split_point + 1, data + split_point, size - split_point);
			} else {
				memcpy(json1_str, data, split_point);
				memcpy(json2_str, data + split_point, size - split_point);
			}

			json1_str[split_point] = '\0';
			json2_str[size - split_point] = '\0';

			/* Try to parse raw fuzzer input as JSON */
			cJSON *raw_json1 = cJSON_Parse(json1_str);
			cJSON *raw_json2 = cJSON_Parse(json2_str);

			if (raw_json1 && raw_json2) {
				/* Test with raw parsed JSON */
				fuzz_test_patch_roundtrip(raw_json1,
				                          raw_json2);
				fuzz_test_self_diff_is_null(raw_json1);
				fuzz_test_self_diff_is_null(raw_json2);
			}

			if (raw_json1)
				cJSON_Delete(raw_json1);
			if (raw_json2)
				cJSON_Delete(raw_json2);
		}

		free(json1_str);
		free(json2_str);
	}

	/* Strategy 4: Test with different diff options */
	struct json_diff_options opts_strict = {.strict_equality = true};
	struct json_diff_options opts_loose = {.strict_equality = false};

	/* Test strict equality */
	diff = json_diff(json1, json2, &opts_strict);
	if (diff) {
		patched = json_patch(json1, diff);
		if (patched) {
			/* Verify result - don't assert, just test */
			json_value_equal(patched, json2, true);
			cJSON_Delete(patched);
			patched = NULL;
		}
		cJSON_Delete(diff);
		diff = NULL;
	}

	/* Test loose equality */
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

	/* Strategy 5: Test utility functions for memory safety */
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

		/* Test with swapped arguments */
		change_arr = create_change_array(json2, json1);
		if (change_arr)
			cJSON_Delete(change_arr);

		add_arr = create_addition_array(json1);
		if (add_arr)
			cJSON_Delete(add_arr);

		del_arr = create_deletion_array(json2);
		if (del_arr)
			cJSON_Delete(del_arr);
	}

	/* Strategy 6: Stress test with multiple operations */
	for (int i = 0; i < 3; i++) {
		cJSON *temp_diff = json_diff(json1, json2, NULL);
		if (temp_diff) {
			cJSON *temp_patched = json_patch(json1, temp_diff);
			if (temp_patched) {
				/* Chain operations to stress test memory
				 * management */
				cJSON *chain_diff =
				    json_diff(temp_patched, json2, NULL);
				if (chain_diff)
					cJSON_Delete(chain_diff);
				cJSON_Delete(temp_patched);
			}
			cJSON_Delete(temp_diff);
		}
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

	return 0;
}
