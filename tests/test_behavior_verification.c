// SPDX-License-Identifier: Apache-2.0
/**
 * test_behavior_verification.c - Test to understand and verify our JSON diff
 * behavior
 *
 * This test verifies what our library actually produces and documents the
 * behavior, rather than assuming it matches jsondiffpatch exactly.
 */

#define __STDC_WANT_LIB_EXT1__ 1
#include "src/json_diff.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Test case structure to document our actual behavior
 */
struct behavior_test {
	const char *name;
	const char *json_a;
	const char *json_b;
};

/**
 * Test cases based on the JavaScript library tests
 */
static const struct behavior_test behavior_tests[] = {
    {.name = "simple object value change",
     .json_a = "{\"1\":1}",
     .json_b = "{\"1\":2}"},
    {.name = "array element change",
     .json_a = "{\"1\":[1,2,3]}",
     .json_b = "{\"1\":[1,2,4]}"},
    {.name = "array element removal",
     .json_a = "{\"1\":[1,2,3]}",
     .json_b = "{\"1\":[2,3]}"},
    {.name = "array element type change",
     .json_a = "{\"1\":[1]}",
     .json_b = "{\"1\":[{\"1\":2}]}"},
    {.name = "complex array with object change",
     .json_a = "{\"1\":[1,{\"1\":1}]}",
     .json_b = "{\"1\":[{\"1\":2}]}"},
    {.name = "nested object change",
     .json_a = "{\"a\":{\"x\":1},\"b\":2}",
     .json_b = "{\"a\":{\"x\":2},\"b\":2}"},
    {.name = "array object element change",
     .json_a = "{\"1\":[{\"1\":1}]}",
     .json_b = "{\"1\":[{\"1\":2}]}"}};

/**
 * Test and document what our library produces
 */
static void test_and_document_behavior(const struct behavior_test *test_case)
{
	printf("=== %s ===\n", test_case->name);
	printf("Input A: %s\n", test_case->json_a);
	printf("Input B: %s\n", test_case->json_b);

	/* Parse input JSONs */
	cJSON *json_a = cJSON_Parse(test_case->json_a);
	cJSON *json_b = cJSON_Parse(test_case->json_b);

	if (!json_a || !json_b) {
		printf("ERROR: Failed to parse input JSONs\n");
		if (json_a)
			cJSON_Delete(json_a);
		if (json_b)
			cJSON_Delete(json_b);
		return;
	}

	/* Compute diff */
	cJSON *diff = json_diff(json_a, json_b, NULL);

	if (!diff) {
		printf("Result: No diff (objects are identical)\n");
	} else {
		char *diff_str = cJSON_Print(diff);
		printf("Our diff: %s\n", diff_str ? diff_str : "NULL");
		free(diff_str);

		/* Test patch functionality */
		cJSON *patched = json_patch(json_a, diff);
		if (patched) {
			/* Validate the patched object before using it */
			if (!patched || !cJSON_IsObject(patched)) {
				printf("Patch test: ✗ FAIL - patch returned "
				       "invalid object\n");
			} else if (json_value_equal(patched, json_b, false)) {
				printf("Patch test: ✓ PASS - patch produces "
				       "expected result\n");
			} else {
				printf("Patch test: ✗ FAIL - patch does not "
				       "produce expected result\n");
				/* Only print if the object seems valid */
				if (patched->type != cJSON_Invalid) {
					char *patched_str =
					    cJSON_Print(patched);
					printf("Patched result: %s\n",
					       patched_str ? patched_str
					                   : "NULL");
					free(patched_str);
				} else {
					printf("Patched result: [INVALID "
					       "OBJECT]\n");
				}
			}
			cJSON_Delete(patched);
		} else {
			printf("Patch test: ✗ FAIL - patch operation failed\n");
		}

		cJSON_Delete(diff);
	}

	cJSON_Delete(json_a);
	cJSON_Delete(json_b);
	printf("\n");
}

/**
 * Test basic functionality that should definitely work
 */
static void test_basic_functionality(void)
{
	printf("=== Basic functionality tests ===\n\n");

	/* Test identical objects */
	printf("--- Identical objects test ---\n");
	const char *identical = "{\"test\":123,\"arr\":[1,2,3]}";
	cJSON *obj1 = cJSON_Parse(identical);
	cJSON *obj2 = cJSON_Parse(identical);

	if (obj1 && obj2) {
		cJSON *diff = json_diff(obj1, obj2, NULL);
		if (diff == NULL) {
			printf("✓ PASS - identical objects produce no diff\n");
		} else {
			printf("✗ FAIL - identical objects should not produce "
			       "a diff\n");
			char *diff_str = cJSON_Print(diff);
			printf("Unexpected diff: %s\n",
			       diff_str ? diff_str : "NULL");
			free(diff_str);
			cJSON_Delete(diff);
		}
		cJSON_Delete(obj1);
		cJSON_Delete(obj2);
	} else {
		printf("✗ FAIL - could not parse test JSON\n");
		if (obj1)
			cJSON_Delete(obj1);
		if (obj2)
			cJSON_Delete(obj2);
	}
	printf("\n");
}

/**
 * Test round-trip functionality
 */
__attribute__((unused)) static void test_roundtrip(void)
{
	printf("=== Round-trip tests ===\n\n");

	for (size_t i = 0;
	     i < sizeof(behavior_tests) / sizeof(behavior_tests[0]); i++) {
		const struct behavior_test *test_case = &behavior_tests[i];

		printf("Round-trip test: %s\n", test_case->name);

		cJSON *json_a = cJSON_Parse(test_case->json_a);
		cJSON *json_b = cJSON_Parse(test_case->json_b);

		if (!json_a || !json_b) {
			printf("  ✗ SKIP - failed to parse inputs\n");
			if (json_a)
				cJSON_Delete(json_a);
			if (json_b)
				cJSON_Delete(json_b);
			continue;
		}

		cJSON *diff = json_diff(json_a, json_b, NULL);

		if (!diff) {
			/* No diff means objects are identical */
			if (json_value_equal(json_a, json_b, false)) {
				printf("  ✓ PASS - no diff for identical "
				       "objects\n");
			} else {
				printf("  ✗ FAIL - no diff but objects are "
				       "different\n");
			}
		} else {
			/* Apply patch and verify result */
			cJSON *patched = json_patch(json_a, diff);
			if (patched) {
				if (json_value_equal(patched, json_b, false)) {
					printf("  ✓ PASS - round-trip "
					       "successful\n");
				} else {
					printf("  ✗ FAIL - patch does not "
					       "produce original result\n");
				}
				cJSON_Delete(patched);
			} else {
				printf("  ✗ FAIL - patch operation failed\n");
			}
			cJSON_Delete(diff);
		}

		cJSON_Delete(json_a);
		cJSON_Delete(json_b);
	}
	printf("\n");
}

int main(void)
{
	printf("=== JSON Diff Behavior Verification ===\n\n");

	/* Test basic functionality first */
	test_basic_functionality();

	/* Document what our library produces */
	for (size_t i = 0;
	     i < sizeof(behavior_tests) / sizeof(behavior_tests[0]); i++) {
		test_and_document_behavior(&behavior_tests[i]);
	}

	/* Test round-trip functionality */
	test_roundtrip();

	printf("=== Behavior verification complete ===\n");
	return 0;
}
