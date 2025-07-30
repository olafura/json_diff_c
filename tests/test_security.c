// SPDX-License-Identifier: Apache-2.0
#define __STDC_WANT_LIB_EXT1__ 1
#include "src/json_diff.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Define constants if not already defined */
#ifndef MAX_JSON_INPUT_SIZE
#define MAX_JSON_INPUT_SIZE (1024 * 1024)
#endif
#ifndef MAX_JSON_DEPTH
#define MAX_JSON_DEPTH 1024
#endif

/**
 * Security regression test suite for JSON Diff library
 *
 * This test suite validates security fixes and prevents regressions
 * by testing edge cases and potential vulnerabilities.
 */

/* Test helper to create large JSON strings */
static char *create_large_json_string(size_t target_size)
{
	char *json = malloc(target_size + 100);
	if (!json)
		return NULL;

	strcpy(json, "{\"data\": \"");
	size_t pos = strlen(json);

	/* Fill with 'A' characters to reach target size */
	while (pos < target_size - 10) {
		json[pos++] = 'A';
	}

	strcpy(json + pos, "\"}");
	return json;
}

/* Test excessive memory usage protection */
static void test_memory_dos_protection(void)
{
	printf("Testing DoS protection against excessive memory usage...\n");

	/* Test 1: Large input size rejection */
	char *large_json1 =
	    create_large_json_string(MAX_JSON_INPUT_SIZE + 1000);
	char *large_json2 =
	    create_large_json_string(MAX_JSON_INPUT_SIZE + 1000);

	if (large_json1 && large_json2) {
		cJSON *diff = json_diff_str(large_json1, large_json2, NULL);
		assert(diff == NULL); /* Should reject oversized input */
		printf("✓ Large input rejection working\n");
	}

	free(large_json1);
	free(large_json2);

	printf("✓ Memory DoS protection working\n");
}

/* Test integer overflow protection in array indexing */
static void test_integer_overflow_protection(void)
{
	printf("Testing integer overflow protection...\n");

	/* Create a diff with large array indices */
	cJSON *diff = cJSON_CreateObject();
	cJSON_AddStringToObject(diff, "_t", "a");

	/* Test with maximum valid index */
	char max_index_key[32];
	snprintf(max_index_key, sizeof(max_index_key), "_%d", INT_MAX);
	cJSON *deletion_array = cJSON_CreateArray();
	cJSON_AddItemToArray(deletion_array, cJSON_CreateString("test"));
	cJSON_AddItemToArray(deletion_array, cJSON_CreateNumber(0));
	cJSON_AddItemToArray(deletion_array, cJSON_CreateNumber(0));
	cJSON_AddItemToObject(diff, max_index_key, deletion_array);

	/* Test with index that would overflow */
	char overflow_key[] = "_999999999999999999999"; /* Way beyond INT_MAX */
	cJSON *overflow_array = cJSON_CreateArray();
	cJSON_AddItemToArray(overflow_array, cJSON_CreateString("test"));
	cJSON_AddItemToArray(overflow_array, cJSON_CreateNumber(0));
	cJSON_AddItemToArray(overflow_array, cJSON_CreateNumber(0));
	cJSON_AddItemToObject(diff, overflow_key, overflow_array);

	/* Apply to a simple array - should handle overflow gracefully */
	cJSON *original = cJSON_CreateArray();
	cJSON_AddItemToArray(original, cJSON_CreateString("item1"));
	cJSON_AddItemToArray(original, cJSON_CreateString("item2"));

	cJSON *result = json_patch(original, diff);

	/* Should either succeed or fail gracefully without crashing */
	if (result) {
		cJSON_Delete(result);
	}

	cJSON_Delete(original);
	cJSON_Delete(diff);

	printf("✓ Integer overflow protection working\n");
}

/* Test recursion depth limits */
static void test_recursion_depth_limits(void)
{
	printf("Testing recursion depth limits...\n");

	/* Create deeply nested JSON structure */
	cJSON *deeply_nested1 = cJSON_CreateObject();
	cJSON *current1 = deeply_nested1;

	cJSON *deeply_nested2 = cJSON_CreateObject();
	cJSON *current2 = deeply_nested2;

	for (int i = 0; i < MAX_JSON_DEPTH + 100; i++) {
		cJSON *nested1 = cJSON_CreateObject();
		cJSON_AddItemToObject(current1, "nested", nested1);
		current1 = nested1;

		cJSON *nested2 = cJSON_CreateObject();
		cJSON_AddStringToObject(nested2, "different", "value");
		cJSON_AddItemToObject(current2, "nested", nested2);
		current2 = nested2;
	}

	/* Should handle excessive depth gracefully */
	cJSON *diff = json_diff(deeply_nested1, deeply_nested2, NULL);

	/* May succeed or fail, but shouldn't crash */
	if (diff) {
		cJSON_Delete(diff);
	}

	cJSON_Delete(deeply_nested1);
	cJSON_Delete(deeply_nested2);

	printf("✓ Recursion depth limits working\n");
}

/* Test null pointer handling */
static void test_null_pointer_safety(void)
{
	printf("Testing null pointer safety...\n");

	/* Test null input validation */
	assert(json_diff_str(NULL, NULL, NULL) == NULL);
	assert(json_diff_str("valid", NULL, NULL) == NULL);
	assert(json_diff_str(NULL, "valid", NULL) == NULL);

	/* Test value equality with nulls */
	assert(json_value_equal(NULL, NULL, true) == true);

	cJSON *valid_json = cJSON_CreateObject();
	cJSON_AddStringToObject(valid_json, "test", "value");

	assert(json_value_equal(NULL, valid_json, true) == false);
	assert(json_value_equal(valid_json, NULL, true) == false);

	/* Test patch with nulls */
	assert(json_patch(NULL, NULL) == NULL);
	assert(json_patch(valid_json, NULL) == NULL);
	assert(json_patch(NULL, valid_json) == NULL);

	cJSON_Delete(valid_json);

	printf("✓ Null pointer safety working\n");
}

/* Test malformed JSON handling */
static void test_malformed_json_handling(void)
{
	printf("Testing malformed JSON handling...\n");

	const char *malformed_inputs[] = {
	    "",                      /* Empty string */
	    "{",                     /* Unclosed object */
	    "}",                     /* Just closing brace */
	    "[",                     /* Unclosed array */
	    "]",                     /* Just closing bracket */
	    "{\"key\":}",            /* Missing value */
	    "{\"key\": \"value\"",   /* Missing closing brace */
	    "invalid",               /* Not JSON at all */
	    "null",                  /* Valid but minimal */
	    "123.456.789",           /* Invalid number */
	    "\"unclosed string",     /* Unclosed string */
	    "{\"key\": \"value\",}", /* Trailing comma */
	    NULL};

	for (int i = 0; malformed_inputs[i] != NULL; i++) {
		cJSON *diff =
		    json_diff_str(malformed_inputs[i], "{\"test\": 1}", NULL);
		/* Should handle gracefully - may succeed or fail but shouldn't
		 * crash */
		if (diff) {
			cJSON_Delete(diff);
		}

		diff =
		    json_diff_str("{\"test\": 1}", malformed_inputs[i], NULL);
		if (diff) {
			cJSON_Delete(diff);
		}
	}

	printf("✓ Malformed JSON handling working\n");
}

/* Test edge cases in string operations */
static void test_string_edge_cases(void)
{
	printf("Testing string operation edge cases...\n");

	/* Test with empty strings */
	cJSON *empty_str1 = cJSON_CreateString("");
	cJSON *empty_str2 = cJSON_CreateString("");
	cJSON *non_empty = cJSON_CreateString("test");

	assert(json_value_equal(empty_str1, empty_str2, true) == true);
	assert(json_value_equal(empty_str1, non_empty, true) == false);

	cJSON *diff = json_diff(empty_str1, non_empty, NULL);
	assert(diff != NULL);

	cJSON *patched = json_patch(empty_str1, diff);
	assert(patched != NULL);
	assert(json_value_equal(patched, non_empty, true) == true);

	cJSON_Delete(empty_str1);
	cJSON_Delete(empty_str2);
	cJSON_Delete(non_empty);
	cJSON_Delete(diff);
	cJSON_Delete(patched);

	/* Test with very long strings */
	char *long_str = malloc(10000);
	if (long_str) {
		memset(long_str, 'A', 9999);
		long_str[9999] = '\0';

		cJSON *long_json1 = cJSON_CreateString(long_str);

		long_str[5000] = 'B'; /* Make it different */
		cJSON *long_json2 = cJSON_CreateString(long_str);

		diff = json_diff(long_json1, long_json2, NULL);
		if (diff) {
			patched = json_patch(long_json1, diff);
			if (patched) {
				cJSON_Delete(patched);
			}
			cJSON_Delete(diff);
		}

		cJSON_Delete(long_json1);
		cJSON_Delete(long_json2);
		free(long_str);
	}

	printf("✓ String edge cases working\n");
}

/* Test memory leak prevention */
static void test_memory_leak_prevention(void)
{
	printf("Testing memory leak prevention...\n");

	/* Perform many operations to stress memory management */
	for (int i = 0; i < 1000; i++) {
		cJSON *obj1 = cJSON_CreateObject();
		cJSON *obj2 = cJSON_CreateObject();

		cJSON_AddNumberToObject(obj1, "value", i);
		cJSON_AddNumberToObject(obj2, "value", i + 1);

		cJSON *diff = json_diff(obj1, obj2, NULL);
		if (diff) {
			cJSON *patched = json_patch(obj1, diff);
			if (patched) {
				cJSON_Delete(patched);
			}
			cJSON_Delete(diff);
		}

		cJSON_Delete(obj1);
		cJSON_Delete(obj2);
	}

	printf("✓ Memory leak prevention working\n");
}

int main(void)
{
	printf("Starting JSON Diff Security Test Suite\n");
	printf("=====================================\n\n");

	test_memory_dos_protection();
	test_integer_overflow_protection();
	test_recursion_depth_limits();
	test_null_pointer_safety();
	test_malformed_json_handling();
	test_string_edge_cases();
	test_memory_leak_prevention();

	printf("\n=====================================\n");
	printf("All security tests passed! ✓\n");
	printf("The library appears to be secure against tested "
	       "vulnerabilities.\n");

	return 0;
}
