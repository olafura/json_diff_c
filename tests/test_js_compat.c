// SPDX-License-Identifier: Apache-2.0
/**
 * test_js_compat.c - Test JSON diff library against jsondiffpatch JavaScript
 * library behavior
 *
 * This file contains tests that verify our C implementation matches the
 * behavior of the jsondiffpatch JavaScript library. Test cases are based on the
 * Go test file from ../jsondiffgo/js_compare_test.go
 */

#define __STDC_WANT_LIB_EXT1__ 1
#include "src/json_diff.h"
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Test case structure for jsondiffpatch compatibility tests
 */
struct js_compat_test {
	const char *name;
	const char *json_a;
	const char *json_b;
	const char *expected_diff; /* NULL if no diff expected */
};

/**
 * Test cases matching the Go test file from jsondiffgo
 */
static const struct js_compat_test js_compat_tests[] = {
    {.name = "simple object value change",
     .json_a = "{\"1\":1}",
     .json_b = "{\"1\":2}",
     .expected_diff = "{\"1\":[1,2]}"},
    {.name = "array element change",
     .json_a = "{\"1\":[1,2,3]}",
     .json_b = "{\"1\":[1,2,4]}",
     .expected_diff = "{\"1\":{\"2\":[4],\"_2\":[3,0,0],\"_t\":\"a\"}}"},
    {.name = "array element removal",
     .json_a = "{\"1\":[1,2,3]}",
     .json_b = "{\"1\":[2,3]}",
     .expected_diff = "{\"1\":{\"_0\":[1,0,0],\"_t\":\"a\"}}"},
    {.name = "array element type change",
     .json_a = "{\"1\":[1]}",
     .json_b = "{\"1\":[{\"1\":2}]}",
     .expected_diff =
         "{\"1\":{\"0\":[{\"1\":2}],\"_0\":[1,0,0],\"_t\":\"a\"}}"},
    {.name = "complex array with object change",
     .json_a = "{\"1\":[1,{\"1\":1}]}",
     .json_b = "{\"1\":[{\"1\":2}]}",
     .expected_diff = "{\"1\":{\"0\":[{\"1\":2}],\"_0\":[1,0,0],\"_1\":[{\"1\":"
                      "1},0,0],\"_t\":\"a\"}}"},
    {.name = "nested object change",
     .json_a = "{\"a\":{\"x\":1},\"b\":2}",
     .json_b = "{\"a\":{\"x\":2},\"b\":2}",
     .expected_diff = "{\"a\":{\"x\":[1,2]}}"},
    {.name = "array object element change",
     .json_a = "{\"1\":[{\"1\":1}]}",
     .json_b = "{\"1\":[{\"1\":2}]}",
     .expected_diff = "{\"1\":{\"0\":{\"1\":[1,2]},\"_t\":\"a\"}}"}};

/**
 * Compare two cJSON objects for structural equality using library function
 */
static bool cjson_equal(const cJSON *a, const cJSON *b)
{
	if (!a && !b)
		return true;
	if (!a || !b)
		return false;

	return json_value_equal(a, b, false);
}

/**
 * Test a single jsondiffpatch compatibility case
 */
static void test_js_compat_case(const struct js_compat_test *test_case)
{
	printf("=== Testing: %s ===\n", test_case->name);
	printf("  Input A: %s\n", test_case->json_a);
	printf("  Input B: %s\n", test_case->json_b);
	fflush(stdout); /* Ensure output is flushed before potential crash */

	/* Parse input JSONs */
	cJSON *json_a = cJSON_Parse(test_case->json_a);
	cJSON *json_b = cJSON_Parse(test_case->json_b);

	if (!json_a || !json_b) {
		printf("ERROR: Failed to parse input JSONs\n");
		printf("  json_a: %s\n", test_case->json_a);
		printf("  json_b: %s\n", test_case->json_b);
		if (json_a)
			cJSON_Delete(json_a);
		if (json_b)
			cJSON_Delete(json_b);
		return;
	}

	/* Compute diff */
	printf("  Computing diff...\n");
	fflush(stdout);
	cJSON *diff = json_diff(json_a, json_b, NULL);
	printf("  Diff computed: %p\n", (void *)diff);
	fflush(stdout);

	/* Print the diff to see what's being generated */
	if (diff) {
		char *diff_str = cJSON_Print(diff);
		printf("  Diff content: %s\n", diff_str ? diff_str : "NULL");
		free(diff_str);
		fflush(stdout);
	}

	if (!test_case->expected_diff) {
		/* No diff expected */
		if (diff != NULL) {
			printf("ERROR: Expected no diff, but got:\n");
			char *diff_str = cJSON_Print(diff);
			printf("  %s\n", diff_str ? diff_str : "NULL");
			free(diff_str);
			cJSON_Delete(diff);
			cJSON_Delete(json_a);
			cJSON_Delete(json_b);
			assert(false);
			return;
		}
		printf("  ✓ No diff as expected\n");
	} else {
		/* Diff expected */
		if (!diff) {
			printf("ERROR: Expected diff, but got NULL\n");
			printf("  Expected: %s\n", test_case->expected_diff);
			cJSON_Delete(json_a);
			cJSON_Delete(json_b);
			assert(false);
			return;
		}

		/* Parse expected diff */
		cJSON *expected = cJSON_Parse(test_case->expected_diff);
		if (!expected) {
			printf(
			    "ERROR: Failed to parse expected diff JSON: %s\n",
			    test_case->expected_diff);
			cJSON_Delete(diff);
			cJSON_Delete(json_a);
			cJSON_Delete(json_b);
			assert(false);
			return;
		}

		/* Compare diff with expected */
		if (!cjson_equal(diff, expected)) {
			printf("ERROR: Diff mismatch\n");
			char *got_str = cJSON_Print(diff);
			char *expected_str = cJSON_Print(expected);
			printf("  Got:      %s\n", got_str ? got_str : "NULL");
			printf("  Expected: %s\n",
			       expected_str ? expected_str : "NULL");
			free(got_str);
			free(expected_str);
			cJSON_Delete(expected);
			cJSON_Delete(diff);
			cJSON_Delete(json_a);
			cJSON_Delete(json_b);
			assert(false);
			return;
		}

		printf("  ✓ Diff matches expected result\n");
		cJSON_Delete(expected);
	}

	/* Test that patch operation works (if diff exists) */
	if (diff) {
		printf("  Testing patch operation...\n");
		fflush(stdout);
		cJSON *patched = json_patch(json_a, diff);
		printf("  Patch computed: %p\n", (void *)patched);
		fflush(stdout);
		if (!patched) {
			printf("ERROR: json_patch failed\n");
			cJSON_Delete(diff);
			cJSON_Delete(json_a);
			cJSON_Delete(json_b);
			return;
		}

		printf("  Verifying patch result...\n");
		fflush(stdout);
		/* Verify patch result equals json_b */
		if (!patched || patched->type == cJSON_Invalid) {
			printf("ERROR: Patched object is invalid\n");
			cJSON_Delete(patched);
			cJSON_Delete(diff);
			cJSON_Delete(json_a);
			cJSON_Delete(json_b);
			return;
		}

		printf("  Calling cjson_equal...\n");
		fflush(stdout);
		bool equal = cjson_equal(patched, json_b);
		printf("  cjson_equal result: %d\n", equal);
		fflush(stdout);

		if (!equal) {
			printf("ERROR: Patch result does not match expected\n");

			printf("  Trying to print json_b first...\n");
			fflush(stdout);
			/* Check if expected object is valid before printing */
			if (!json_b || json_b->type == cJSON_Invalid) {
				printf("  Expected: [INVALID OBJECT]\n");
			} else {
				char *expected_str = cJSON_Print(json_b);
				printf("  Expected: %s\n",
				       expected_str ? expected_str : "NULL");
				free(expected_str);
			}

			printf("  Trying to print patched object...\n");
			fflush(stdout);
			/* Check if patched object is valid before printing */
			if (!patched || patched->type == cJSON_Invalid) {
				printf("  Patched:  [INVALID OBJECT]\n");
			} else {
				cJSON *copy_patched =
				    cJSON_Duplicate(patched, 1);
				char *patched_str = cJSON_Print(copy_patched);
				printf("  Patched:  %s\n",
				       patched_str ? patched_str : "NULL");
				free(patched_str);
				free(copy_patched);
			}

			cJSON_Delete(patched);
			cJSON_Delete(diff);
			cJSON_Delete(json_a);
			cJSON_Delete(json_b);
			assert(false);
			return;
		}

		printf("  ✓ Patch operation successful\n");
		cJSON_Delete(patched);
		cJSON_Delete(diff);
	}

	cJSON_Delete(json_a);
	cJSON_Delete(json_b);

	printf("  ✓ Test passed\n\n");
}

/**
 * Test identical objects (should produce no diff)
 */
static void test_identical_objects(void)
{
	printf("Testing identical objects...\n");

	const char *json_str = "{\"a\":1,\"b\":[1,2,3],\"c\":{\"x\":\"test\"}}";
	cJSON *json_a = cJSON_Parse(json_str);
	cJSON *json_b = cJSON_Parse(json_str);

	assert(json_a && json_b);

	cJSON *diff = json_diff(json_a, json_b, NULL);
	if (diff != NULL) {
		printf("ERROR: Expected no diff for identical objects\n");
		char *diff_str = cJSON_Print(diff);
		printf("  Got: %s\n", diff_str ? diff_str : "NULL");
		free(diff_str);
		cJSON_Delete(diff);
		assert(false);
	}

	cJSON_Delete(json_a);
	cJSON_Delete(json_b);

	printf("  ✓ Identical objects produce no diff\n\n");
}

/**
 * Test edge cases
 */
static void test_edge_cases(void)
{
	printf("Testing edge cases...\n");

	/* Test null values */
	cJSON *null_a = cJSON_Parse("null");
	cJSON *null_b = cJSON_Parse("null");
	assert(null_a && null_b);

	cJSON *diff = json_diff(null_a, null_b, NULL);
	assert(diff == NULL); /* No diff for identical nulls */

	cJSON_Delete(null_a);
	cJSON_Delete(null_b);

	/* Test empty objects */
	cJSON *empty_a = cJSON_Parse("{}");
	cJSON *empty_b = cJSON_Parse("{}");
	assert(empty_a && empty_b);

	diff = json_diff(empty_a, empty_b, NULL);
	assert(diff == NULL); /* No diff for identical empty objects */

	cJSON_Delete(empty_a);
	cJSON_Delete(empty_b);

	/* Test empty arrays */
	cJSON *empty_arr_a = cJSON_Parse("[]");
	cJSON *empty_arr_b = cJSON_Parse("[]");
	assert(empty_arr_a && empty_arr_b);

	diff = json_diff(empty_arr_a, empty_arr_b, NULL);
	assert(diff == NULL); /* No diff for identical empty arrays */

	cJSON_Delete(empty_arr_a);
	cJSON_Delete(empty_arr_b);

	printf("  ✓ Edge cases handled correctly\n\n");
}

/**
 * Test string diff functionality
 */
static void test_js_compat_strings(void)
{
	printf("Testing jsondiffpatch string compatibility...\n");

	/* Test all compatibility cases using string interface */
	for (size_t i = 0;
	     i < sizeof(js_compat_tests) / sizeof(js_compat_tests[0]); i++) {
		const struct js_compat_test *test_case = &js_compat_tests[i];

		printf("String test: %s\n", test_case->name);

		/* Use json_diff_str function */
		cJSON *diff =
		    json_diff_str(test_case->json_a, test_case->json_b, NULL);

		if (!test_case->expected_diff) {
			/* No diff expected */
			if (diff != NULL) {
				printf(
				    "ERROR: Expected no diff, but got diff\n");
				char *diff_str = cJSON_Print(diff);
				printf("  %s\n", diff_str ? diff_str : "NULL");
				free(diff_str);
				cJSON_Delete(diff);
				assert(false);
			}
		} else {
			/* Diff expected */
			if (!diff) {
				printf("ERROR: Expected diff, but got NULL\n");
				assert(false);
			}

			cJSON *expected = cJSON_Parse(test_case->expected_diff);
			assert(expected);

			if (!cjson_equal(diff, expected)) {
				printf("ERROR: String diff mismatch\n");
				char *got_str = cJSON_Print(diff);
				char *expected_str = cJSON_Print(expected);
				printf("  Got:      %s\n",
				       got_str ? got_str : "NULL");
				printf("  Expected: %s\n",
				       expected_str ? expected_str : "NULL");
				free(got_str);
				free(expected_str);
				cJSON_Delete(expected);
				cJSON_Delete(diff);
				assert(false);
			}

			cJSON_Delete(expected);
			cJSON_Delete(diff);
		}

		printf("  ✓ String test passed\n");
	}

	printf("  ✓ All string compatibility tests passed\n\n");
}

/**
 * Main test function
 */
int main(void)
{
	printf("=== JSON Diff jsondiffpatch Compatibility Tests ===\n\n");

	/* Run all compatibility test cases */
	for (size_t i = 0;
	     i < sizeof(js_compat_tests) / sizeof(js_compat_tests[0]); i++) {
		test_js_compat_case(&js_compat_tests[i]);
	}

	/* Run additional tests */
	test_identical_objects();
	test_edge_cases();
	test_js_compat_strings();

	printf("=== All jsondiffpatch compatibility tests passed! ===\n");
	return 0;
}
