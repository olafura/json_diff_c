// SPDX-License-Identifier: Apache-2.0
// Property-based testing for specific issues found in json_diff
#include "src/json_diff.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Test for memory corruption issues with change arrays
 * This test was inspired by the bug where missing cJSON_IsNumber
 * caused numbers to become null in change arrays
 */
static void test_number_corruption_property(void)
{
	printf("Testing number corruption property...\n");

	/* Test various number combinations that could trigger corruption */
	double test_numbers[] = {0.0,
	                         1.0,
	                         -1.0,
	                         42.0,
	                         -42.0,
	                         123.456,
	                         -123.456,
	                         1e10,
	                         -1e10,
	                         1e-10,
	                         -1e-10,
	                         __builtin_inf(),
	                         -__builtin_inf()};

	size_t num_tests = sizeof(test_numbers) / sizeof(test_numbers[0]);

	for (size_t i = 0; i < num_tests; i++) {
		for (size_t j = 0; j < num_tests; j++) {
			if (i == j)
				continue; /* Skip identical numbers */

			/* Create test objects with these numbers */
			cJSON *obj1 = cJSON_CreateObject();
			cJSON *obj2 = cJSON_CreateObject();
			cJSON_AddNumberToObject(obj1, "value", test_numbers[i]);
			cJSON_AddNumberToObject(obj2, "value", test_numbers[j]);

			/* Generate diff */
			cJSON *diff = json_diff(obj1, obj2, NULL);
			assert(diff != NULL);

			/* Verify diff structure */
			cJSON *value_diff = cJSON_GetObjectItem(diff, "value");
			assert(value_diff != NULL);
			assert(cJSON_IsArray(value_diff));
			assert(cJSON_GetArraySize(value_diff) == 2);

			/* Verify the numbers are preserved correctly */
			cJSON *old_val = cJSON_GetArrayItem(value_diff, 0);
			cJSON *new_val = cJSON_GetArrayItem(value_diff, 1);
			assert(old_val != NULL && cJSON_IsNumber(old_val));
			assert(new_val != NULL && cJSON_IsNumber(new_val));

			/* Check values are correct (handling NaN/Inf specially)
			 */
			if (!__builtin_isinf(test_numbers[i]) &&
			    !__builtin_isnan(test_numbers[i])) {
				assert(old_val->valuedouble == test_numbers[i]);
			}
			if (!__builtin_isinf(test_numbers[j]) &&
			    !__builtin_isnan(test_numbers[j])) {
				assert(new_val->valuedouble == test_numbers[j]);
			}

			/* Test roundtrip property */
			cJSON *patched = json_patch(obj1, diff);
			assert(patched != NULL);
			/* For numbers, allow small floating point differences */
			if (cJSON_IsObject(obj2)) {
				cJSON *patched_val = cJSON_GetObjectItem(patched, "value");
				cJSON *expected_val = cJSON_GetObjectItem(obj2, "value");
				if (patched_val && expected_val && cJSON_IsNumber(patched_val) && cJSON_IsNumber(expected_val)) {
					assert(fabs(patched_val->valuedouble - expected_val->valuedouble) < 1e-9);
				} else {
					assert(json_value_equal(patched, obj2, false));
				}
			} else {
				assert(json_value_equal(patched, obj2, false));
			}

			/* Cleanup */
			cJSON_Delete(obj1);
			cJSON_Delete(obj2);
			cJSON_Delete(diff);
			cJSON_Delete(patched);
		}
	}

	printf("Number corruption property tests passed!\n");
}

/**
 * Test for array diff format consistency
 * Ensures array diffs follow the expected format: addition [new] and deletion
 * [old, 0, 0]
 */
static void test_array_diff_format_property(void)
{
	printf("Testing array diff format property...\n");

	/* Test various array transformations */
	struct {
		const char *name;
		const char *left_json;
		const char *right_json;
	} test_cases[] = {
	    {"single_element_change", "{\"arr\": [1, 2, 3]}",
	     "{\"arr\": [1, 2, 4]}"},
	    {"add_element", "{\"arr\": [1, 2]}", "{\"arr\": [1, 2, 3]}"},
	    {"remove_element", "{\"arr\": [1, 2, 3]}", "{\"arr\": [1, 2]}"},
	    {"multiple_changes", "{\"arr\": [1, 2, 3, 4]}",
	     "{\"arr\": [5, 2, 6, 7]}"},
	    {"empty_to_full", "{\"arr\": []}", "{\"arr\": [1, 2, 3]}"},
	    {"full_to_empty", "{\"arr\": [1, 2, 3]}", "{\"arr\": []}"}};

	size_t num_cases = sizeof(test_cases) / sizeof(test_cases[0]);

	for (size_t i = 0; i < num_cases; i++) {
		printf("  Testing %s...\n", test_cases[i].name);

		cJSON *left = cJSON_Parse(test_cases[i].left_json);
		cJSON *right = cJSON_Parse(test_cases[i].right_json);
		assert(left && right);

		cJSON *diff = json_diff(left, right, NULL);
		if (!diff) {
			/* Arrays are equal, which is valid */
			assert(json_value_equal(left, right, false));
		} else {
			/* Verify array diff structure */
			cJSON *arr_diff = cJSON_GetObjectItem(diff, "arr");
			assert(arr_diff != NULL);
			assert(cJSON_IsObject(arr_diff));

			/* Must have array marker */
			cJSON *marker = cJSON_GetObjectItem(arr_diff, "_t");
			assert(marker != NULL);
			assert(cJSON_IsString(marker));
			assert(strcmp(marker->valuestring, "a") == 0);

			/* Check that additions and deletions follow correct
			 * format */
			cJSON *item = arr_diff->child;
			while (item) {
				if (strcmp(item->string, "_t") == 0) {
					item = item->next;
					continue;
				}

				assert(cJSON_IsArray(item));

				if (item->string[0] == '_') {
					/* Deletion: should be [old_value, 0, 0]
					 */
					assert(cJSON_GetArraySize(item) == 3);
					cJSON *zero1 =
					    cJSON_GetArrayItem(item, 1);
					cJSON *zero2 =
					    cJSON_GetArrayItem(item, 2);
					assert(cJSON_IsNumber(zero1) &&
					       zero1->valuedouble == 0.0);
					assert(cJSON_IsNumber(zero2) &&
					       zero2->valuedouble == 0.0);
				} else {
					/* Addition or change: verify it's a
					 * valid format */
					int size = cJSON_GetArraySize(item);
					assert(size == 1 ||
					       size ==
					           2); /* [new] or [old, new] */
				}

				item = item->next;
			}

			/* Test roundtrip property */
			cJSON *patched = json_patch(left, diff);
			assert(patched != NULL);
			assert(json_value_equal(patched, right, false));

			cJSON_Delete(patched);
		}

		cJSON_Delete(left);
		cJSON_Delete(right);
		if (diff)
			cJSON_Delete(diff);
	}

	printf("Array diff format property tests passed!\n");
}

/**
 * Test for deep nesting stability
 * Ensures deeply nested structures don't cause stack overflow or corruption
 */
static void test_deep_nesting_property(void)
{
	printf("Testing deep nesting property...\n");

	/* Create deeply nested objects */
	for (int depth = 1; depth <= 20; depth++) {
		cJSON *obj1 = cJSON_CreateObject();
		cJSON *obj2 = cJSON_CreateObject();
		cJSON *current1 = obj1;
		cJSON *current2 = obj2;

		/* Build nested structure */
		for (int i = 0; i < depth; i++) {
			cJSON *nested1 = cJSON_CreateObject();
			cJSON *nested2 = cJSON_CreateObject();

			cJSON_AddItemToObject(current1, "nested", nested1);
			cJSON_AddItemToObject(current2, "nested", nested2);

			current1 = nested1;
			current2 = nested2;
		}

		/* Add different values at the deepest level */
		cJSON_AddNumberToObject(current1, "value", 1.0);
		cJSON_AddNumberToObject(current2, "value", 2.0);

		/* Test diff and patch */
		cJSON *diff = json_diff(obj1, obj2, NULL);
		assert(diff != NULL);

		cJSON *patched = json_patch(obj1, diff);
		assert(patched != NULL);
		assert(json_value_equal(patched, obj2, false));

		/* Cleanup */
		cJSON_Delete(obj1);
		cJSON_Delete(obj2);
		cJSON_Delete(diff);
		cJSON_Delete(patched);
	}

	printf("Deep nesting property tests passed!\n");
}

/**
 * Test for string handling edge cases
 * Ensures strings with special characters, escapes, etc. are handled correctly
 */
static void test_string_handling_property(void)
{
	printf("Testing string handling property...\n");

	const char *test_strings[] = {
	    "",
	    "simple",
	    "with spaces",
	    "with\nnewlines",
	    "with\ttabs",
	    "with\"quotes\"",
	    "with\\backslashes",
	    "with/forward/slashes",
	    "with\x01control\x02chars\x03",
	    "unicode: 🚀 ñ ü ß",
	    "very long string: " /* followed by repetition */
	    "abcdefghijklmnopqrstuvwxyz"
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	    "0123456789!@#$%^&*()_+-={}|[]\\:\";'<>?,./"};

	size_t num_strings = sizeof(test_strings) / sizeof(test_strings[0]);

	for (size_t i = 0; i < num_strings; i++) {
		for (size_t j = 0; j < num_strings; j++) {
			if (i == j)
				continue;

			/* Create objects with these strings */
			cJSON *obj1 = cJSON_CreateObject();
			cJSON *obj2 = cJSON_CreateObject();
			cJSON_AddStringToObject(obj1, "str", test_strings[i]);
			cJSON_AddStringToObject(obj2, "str", test_strings[j]);

			/* Test diff */
			cJSON *diff = json_diff(obj1, obj2, NULL);
			assert(diff != NULL);

			/* Verify string values are preserved */
			cJSON *str_diff = cJSON_GetObjectItem(diff, "str");
			assert(str_diff != NULL && cJSON_IsArray(str_diff));
			assert(cJSON_GetArraySize(str_diff) == 2);

			cJSON *old_str = cJSON_GetArrayItem(str_diff, 0);
			cJSON *new_str = cJSON_GetArrayItem(str_diff, 1);
			assert(cJSON_IsString(old_str) &&
			       cJSON_IsString(new_str));
			assert(strcmp(old_str->valuestring, test_strings[i]) ==
			       0);
			assert(strcmp(new_str->valuestring, test_strings[j]) ==
			       0);

			/* Test roundtrip */
			cJSON *patched = json_patch(obj1, diff);
			assert(patched != NULL);
			assert(json_value_equal(patched, obj2, false));

			/* Cleanup */
			cJSON_Delete(obj1);
			cJSON_Delete(obj2);
			cJSON_Delete(diff);
			cJSON_Delete(patched);
		}
	}

	printf("String handling property tests passed!\n");
}

/* Helper functions for type creation */
static cJSON *create_test_number(void) { return cJSON_CreateNumber(42.0); }

static cJSON *create_test_string(void) { return cJSON_CreateString("test"); }

static cJSON *create_test_array(void)
{
	cJSON *arr = cJSON_CreateArray();
	cJSON_AddItemToArray(arr, cJSON_CreateNumber(1));
	return arr;
}

static cJSON *create_test_object(void)
{
	cJSON *obj = cJSON_CreateObject();
	cJSON_AddNumberToObject(obj, "field", 1);
	return obj;
}

/**
 * Test for type mixing scenarios
 * Ensures changes between different JSON types are handled correctly
 */
static void test_type_mixing_property(void)
{
	printf("Testing type mixing property...\n");

	/* Test all combinations of type changes */
	struct {
		const char *name;
		cJSON *(*create_func)(void);
	} types[] = {
	    {"null", cJSON_CreateNull},     {"true", cJSON_CreateTrue},
	    {"false", cJSON_CreateFalse},   {"number", create_test_number},
	    {"string", create_test_string}, {"array", create_test_array},
	    {"object", create_test_object}};

	size_t num_types = sizeof(types) / sizeof(types[0]);

	for (size_t i = 0; i < num_types; i++) {
		for (size_t j = 0; j < num_types; j++) {
			if (i == j)
				continue;

			printf("  Testing %s -> %s...\n", types[i].name,
			       types[j].name);

			/* Create objects with different types */
			cJSON *obj1 = cJSON_CreateObject();
			cJSON *obj2 = cJSON_CreateObject();
			cJSON_AddItemToObject(obj1, "field",
			                      types[i].create_func());
			cJSON_AddItemToObject(obj2, "field",
			                      types[j].create_func());

			/* Test diff */
			cJSON *diff = json_diff(obj1, obj2, NULL);
			assert(diff != NULL);

			/* For type changes, should get change array */
			cJSON *field_diff = cJSON_GetObjectItem(diff, "field");
			assert(field_diff != NULL && cJSON_IsArray(field_diff));
			assert(cJSON_GetArraySize(field_diff) == 2);

			/* Test roundtrip */
			cJSON *patched = json_patch(obj1, diff);
			assert(patched != NULL);
			assert(json_value_equal(patched, obj2, false));

			/* Cleanup */
			cJSON_Delete(obj1);
			cJSON_Delete(obj2);
			cJSON_Delete(diff);
			cJSON_Delete(patched);
		}
	}

	printf("Type mixing property tests passed!\n");
}

int main(void)
{
	printf("JSON Diff Property Testing\n");
	printf("==========================\n\n");

	test_number_corruption_property();
	printf("\n");

	test_array_diff_format_property();
	printf("\n");

	test_deep_nesting_property();
	printf("\n");

	test_string_handling_property();
	printf("\n");

	test_type_mixing_property();
	printf("\n");

	printf("All property tests passed!\n");
	return 0;
}
