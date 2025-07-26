// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <math.h>
#include "json_diff.h"

/**
 * test_basic_diff - Test basic JSON diffing functionality
 */
static void test_basic_diff(void)
{
	struct json_value *obj1, *obj2, *diff;
	struct json_value *val1, *val2;

	printf("Testing basic diff...\n");

	/* Create {"test": 1} */
	obj1 = json_value_create_object();
	val1 = json_value_create_number(1);
	json_object_set(obj1->data.object_val, "test", val1);

	/* Create {"test": 2} */
	obj2 = json_value_create_object();
	val2 = json_value_create_number(2);
	json_object_set(obj2->data.object_val, "test", val2);

	/* Test diff */
	diff = json_diff(obj1, obj2, NULL);
	assert(diff != NULL);
	assert(diff->type == JSON_OBJECT);

	/* Verify diff contains test: [1, 2] */
	struct json_value *test_diff = json_object_get(diff->data.object_val, "test");
	assert(test_diff != NULL);
	assert(test_diff->type == JSON_ARRAY);
	assert(test_diff->data.array_val->count == 2);
	assert(test_diff->data.array_val->values[0].data.number_val == 1);
	assert(test_diff->data.array_val->values[1].data.number_val == 2);

	json_value_free(obj1);
	json_value_free(obj2);
	json_value_free(diff);
	/* val1 and val2 are freed when obj1 and obj2 are freed */

	printf("Basic diff test passed!\n");
}

/**
 * test_array_diff - Test array diffing functionality
 */
static void test_array_diff(void)
{
	struct json_value *obj1, *obj2, *diff;
	struct json_value *arr1, *arr2;
	struct json_value *val1, *val2, *val3;

	printf("Testing array diff...\n");

	/* Create {"test": [1,2,3]} */
	obj1 = json_value_create_object();
	arr1 = json_value_create_array();
	val1 = json_value_create_number(1);
	val2 = json_value_create_number(2);
	val3 = json_value_create_number(3);
	json_array_append(arr1->data.array_val, val1);
	json_array_append(arr1->data.array_val, val2);
	json_array_append(arr1->data.array_val, val3);
	json_object_set(obj1->data.object_val, "test", arr1);

	/* Create {"test": [2,3]} */
	obj2 = json_value_create_object();
	arr2 = json_value_create_array();
	json_array_append(arr2->data.array_val, val2);
	json_array_append(arr2->data.array_val, val3);
	json_object_set(obj2->data.object_val, "test", arr2);

	/* Test diff */
	diff = json_diff(obj1, obj2, NULL);
	assert(diff != NULL);
	assert(diff->type == JSON_OBJECT);

	/* Verify diff structure */
	struct json_value *test_diff = json_object_get(diff->data.object_val, "test");
	assert(test_diff != NULL);
	assert(test_diff->type == JSON_OBJECT);

	/* Should have array marker */
	struct json_value *marker = json_object_get(test_diff->data.object_val, "_t");
	assert(marker != NULL);
	assert(strcmp(marker->data.string_val, "a") == 0);

	json_value_free(obj1);
	json_value_free(obj2);
	json_value_free(diff);
	/* arr1, arr2, val1, val2, val3 are freed when obj1 and obj2 are freed */

	printf("Array diff test passed!\n");
}

/**
 * test_patch - Test JSON patching functionality
 */
static void test_patch(void)
{
	struct json_value *obj1, *obj2, *diff, *patched;
	struct json_value *val1, *val2;

	printf("Testing patch...\n");

	/* Create {"test": 1} */
	obj1 = json_value_create_object();
	val1 = json_value_create_number(1);
	json_object_set(obj1->data.object_val, "test", val1);

	/* Create {"test": 2} */
	obj2 = json_value_create_object();
	val2 = json_value_create_number(2);
	json_object_set(obj2->data.object_val, "test", val2);

	/* Create diff and patch */
	diff = json_diff(obj1, obj2, NULL);
	assert(diff != NULL);

	patched = json_patch(obj1, diff);
	assert(patched != NULL);

	/* Verify patched result equals obj2 */
	assert(json_value_equal(patched, obj2, true));

	json_value_free(obj1);
	json_value_free(obj2);
	json_value_free(diff);
	json_value_free(patched);
	/* val1 and val2 are freed when obj1 and obj2 are freed */

	printf("Patch test passed!\n");
}

/**
 * test_strict_equality - Test strict equality option
 */
static void test_strict_equality(void)
{
	struct json_value *obj1, *obj2, *diff;
	struct json_value *val1, *val2;
	struct json_diff_options opts;

	printf("Testing strict equality...\n");

	/* Create {"test": 4} (integer) */
	obj1 = json_value_create_object();
	val1 = json_value_create_number(4.0);
	json_object_set(obj1->data.object_val, "test", val1);

	/* Create {"test": 4.0} (float) */
	obj2 = json_value_create_object();
	val2 = json_value_create_number(4.0);
	json_object_set(obj2->data.object_val, "test", val2);

	/* Test with strict equality (should be equal) */
	opts.strict_equality = true;
	diff = json_diff(obj1, obj2, &opts);
	assert(diff == NULL); /* Should be equal */

	/* Test with non-strict equality */
	opts.strict_equality = false;
	diff = json_diff(obj1, obj2, &opts);
	assert(diff == NULL); /* Should still be equal for same values */

	json_value_free(obj1);
	json_value_free(obj2);
	/* val1 and val2 are freed when obj1 and obj2 are freed */

	printf("Strict equality test passed!\n");
}

/**
 * test_same_object - Test diffing identical objects
 */
static void test_same_object(void)
{
	struct json_value *obj1, *diff;
	struct json_value *val1;

	printf("Testing same object...\n");

	/* Create {"test": 1} */
	obj1 = json_value_create_object();
	val1 = json_value_create_number(1);
	json_object_set(obj1->data.object_val, "test", val1);

	/* Diff with itself */
	diff = json_diff(obj1, obj1, NULL);
	assert(diff == NULL); /* Should be NULL for identical objects */

	json_value_free(obj1);
	/* val1 is freed when obj1 is freed */

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
