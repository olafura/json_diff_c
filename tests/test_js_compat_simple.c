// SPDX-License-Identifier: Apache-2.0
/**
 * test_js_compat_simple.c - Simplified test for jsondiffpatch compatibility
 *
 * A minimal version of the compatibility test to debug issues
 */

#define __STDC_WANT_LIB_EXT1__ 1
#include "src/json_diff.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Simple test case
 */
static void test_simple_case(void)
{
	printf("Testing simple case: {\"1\":1} -> {\"1\":2}\n");

	const char *json_a = "{\"1\":1}";
	const char *json_b = "{\"1\":2}";
	const char *expected = "{\"1\":[1,2]}";

	printf("  Parsing inputs...\n");
	cJSON *a = cJSON_Parse(json_a);
	cJSON *b = cJSON_Parse(json_b);
	cJSON *expected_diff = cJSON_Parse(expected);

	if (!a || !b || !expected_diff) {
		printf("  ERROR: Failed to parse JSON inputs\n");
		if (a)
			cJSON_Delete(a);
		if (b)
			cJSON_Delete(b);
		if (expected_diff)
			cJSON_Delete(expected_diff);
		return;
	}

	printf("  Computing diff...\n");
	cJSON *diff = json_diff(a, b, NULL);

	if (!diff) {
		printf("  ERROR: json_diff returned NULL\n");
		cJSON_Delete(a);
		cJSON_Delete(b);
		cJSON_Delete(expected_diff);
		return;
	}

	printf("  Comparing result...\n");
	char *diff_str = cJSON_Print(diff);
	char *expected_str = cJSON_Print(expected_diff);

	printf("  Got:      %s\n", diff_str ? diff_str : "NULL");
	printf("  Expected: %s\n", expected_str ? expected_str : "NULL");

	/* Simple structural comparison */
	bool equal = json_value_equal(diff, expected_diff, false);

	if (equal) {
		printf("  ✓ Test passed\n");
	} else {
		printf("  ✗ Test failed - results don't match\n");
	}

	free(diff_str);
	free(expected_str);
	cJSON_Delete(diff);
	cJSON_Delete(expected_diff);
	cJSON_Delete(a);
	cJSON_Delete(b);
}

/**
 * Test identical objects
 */
static void test_identical(void)
{
	printf("Testing identical objects\n");

	const char *json_str = "{\"test\":123}";

	cJSON *a = cJSON_Parse(json_str);
	cJSON *b = cJSON_Parse(json_str);

	if (!a || !b) {
		printf("  ERROR: Failed to parse JSON\n");
		if (a)
			cJSON_Delete(a);
		if (b)
			cJSON_Delete(b);
		return;
	}

	cJSON *diff = json_diff(a, b, NULL);

	if (diff == NULL) {
		printf("  ✓ No diff for identical objects (expected)\n");
	} else {
		printf("  ✗ Unexpected diff for identical objects\n");
		char *diff_str = cJSON_Print(diff);
		printf("    Diff: %s\n", diff_str ? diff_str : "NULL");
		free(diff_str);
		cJSON_Delete(diff);
	}

	cJSON_Delete(a);
	cJSON_Delete(b);
}

int main(void)
{
	printf("=== Simplified jsondiffpatch Compatibility Test ===\n\n");

	test_simple_case();
	printf("\n");
	test_identical();

	printf("\n=== Test complete ===\n");
	return 0;
}
