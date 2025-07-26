// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <math.h>
#include "json_diff.h"

/**
 * test_basic_diff - Test basic JSON diffing functionality
 */
static void test_basic_diff(void)
{
	cJSON *obj1, *obj2, *diff;

	printf("Testing basic diff...\n");

	/* Create {"test": 1} */
	obj1 = cJSON_CreateObject();
	cJSON_AddNumberToObject(obj1, "test", 1);

	/* Create {"test": 2} */
	obj2 = cJSON_CreateObject();
	cJSON_AddNumberToObject(obj2, "test", 2);

	/* Test diff */
	diff = json_diff(obj1, obj2, NULL);
	assert(diff != NULL);
	assert(cJSON_IsObject(diff));

	/* Verify diff contains test: [1, 2] */
	cJSON *test_diff = cJSON_GetObjectItem(diff, "test");
	assert(test_diff != NULL);
	assert(cJSON_IsArray(test_diff));
	assert(cJSON_GetArraySize(test_diff) == 2);
	assert(cJSON_GetArrayItem(test_diff, 0)->valuedouble == 1);
	assert(cJSON_GetArrayItem(test_diff, 1)->valuedouble == 2);

	cJSON_Delete(obj1);
	cJSON_Delete(obj2);
	cJSON_Delete(diff);

	printf("Basic diff test passed!\n");
}

/**
 * test_array_diff - Test array diffing functionality
 */
static void test_array_diff(void)
{
	cJSON *obj1, *obj2, *diff;
	cJSON *arr1, *arr2;

	printf("Testing array diff...\n");

	/* Create {"test": [1,2,3]} */
	obj1 = cJSON_CreateObject();
	arr1 = cJSON_CreateArray();
	cJSON_AddItemToArray(arr1, cJSON_CreateNumber(1));
	cJSON_AddItemToArray(arr1, cJSON_CreateNumber(2));
	cJSON_AddItemToArray(arr1, cJSON_CreateNumber(3));
	cJSON_AddItemToObject(obj1, "test", arr1);

	/* Create {"test": [2,3]} */
	obj2 = cJSON_CreateObject();
	arr2 = cJSON_CreateArray();
	cJSON_AddItemToArray(arr2, cJSON_CreateNumber(2));
	cJSON_AddItemToArray(arr2, cJSON_CreateNumber(3));
	cJSON_AddItemToObject(obj2, "test", arr2);

	/* Test diff */
	diff = json_diff(obj1, obj2, NULL);
	assert(diff != NULL);
	assert(cJSON_IsObject(diff));

	/* Verify diff structure */
	cJSON *test_diff = cJSON_GetObjectItem(diff, "test");
	assert(test_diff != NULL);
	assert(cJSON_IsObject(test_diff));

	/* Should have array marker */
	cJSON *marker = cJSON_GetObjectItem(test_diff, "_t");
	assert(marker != NULL);
	assert(strcmp(marker->valuestring, "a") == 0);

	cJSON_Delete(obj1);
	cJSON_Delete(obj2);
	cJSON_Delete(diff);

	printf("Array diff test passed!\n");
}

/**
 * test_patch - Test JSON patching functionality
 */
static void test_patch(void)
{
	cJSON *obj1, *obj2, *diff, *patched;

	printf("Testing patch...\n");

	/* Create {"test": 1} */
	obj1 = cJSON_CreateObject();
	cJSON_AddNumberToObject(obj1, "test", 1);

	/* Create {"test": 2} */
	obj2 = cJSON_CreateObject();
	cJSON_AddNumberToObject(obj2, "test", 2);

	/* Create diff and patch */
	diff = json_diff(obj1, obj2, NULL);
	assert(diff != NULL);

	patched = json_patch(obj1, diff);
	assert(patched != NULL);

	/* Verify patched result equals obj2 */
	assert(json_value_equal(patched, obj2, true));

	cJSON_Delete(obj1);
	cJSON_Delete(obj2);
	cJSON_Delete(diff);
	cJSON_Delete(patched);

	printf("Patch test passed!\n");
}

/**
 * test_strict_equality - Test strict equality option
 */
static void test_strict_equality(void)
{
	cJSON *obj1, *obj2, *diff;
	struct json_diff_options opts;

	printf("Testing strict equality...\n");

	/* Create {"test": 4} (integer) */
	obj1 = cJSON_CreateObject();
	cJSON_AddNumberToObject(obj1, "test", 4.0);

	/* Create {"test": 4.0} (float) */
	obj2 = cJSON_CreateObject();
	cJSON_AddNumberToObject(obj2, "test", 4.0);

	/* Test with strict equality (should be equal) */
	opts.strict_equality = true;
	diff = json_diff(obj1, obj2, &opts);
	assert(diff == NULL); /* Should be equal */

	/* Test with non-strict equality */
	opts.strict_equality = false;
	diff = json_diff(obj1, obj2, &opts);
	assert(diff == NULL); /* Should still be equal for same values */

	cJSON_Delete(obj1);
	cJSON_Delete(obj2);

	printf("Strict equality test passed!\n");
}

/**
 * test_same_object - Test diffing identical objects
 */
static void test_same_object(void)
{
	cJSON *obj1, *diff;

	printf("Testing same object...\n");

	/* Create {"test": 1} */
	obj1 = cJSON_CreateObject();
	cJSON_AddNumberToObject(obj1, "test", 1);

	/* Diff with itself */
	diff = json_diff(obj1, obj1, NULL);
	assert(diff == NULL); /* Should be NULL for identical objects */

	cJSON_Delete(obj1);

	printf("Same object test passed!\n");
}

/**
 * main - Run all tests
 */
int main(void)
{
	printf("Running JSON diff tests...\n\n");

	test_basic_diff();
	test_array_diff();
	test_patch();
	test_strict_equality();
	test_same_object();

	printf("\nAll tests passed!\n");
	return 0;
}
