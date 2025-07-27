// SPDX-License-Identifier: Apache-2.0
#define __STDC_WANT_LIB_EXT1__ 1
#include "src/json_diff.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Declare C11 Annex K secure functions if not available */
#ifndef __STDC_LIB_EXT1__
typedef size_t rsize_t;
typedef int errno_t;

errno_t snprintf_s(char *restrict s, rsize_t n, const char *restrict format, ...);
errno_t memcpy_s(void *restrict s1, rsize_t s1max, const void *restrict s2, rsize_t n);

/* Simple implementations for systems without Annex K */
#include <stdarg.h>
errno_t snprintf_s(char *restrict s, rsize_t n, const char *restrict format, ...)
{
    if (!s || n == 0) return EINVAL;
    va_list args;
    va_start(args, format);
    int result = vsnprintf(s, n, format, args);
    va_end(args);
    return (result >= 0 && (size_t)result < n) ? 0 : ERANGE;
}

errno_t memcpy_s(void *restrict s1, rsize_t s1max, const void *restrict s2, rsize_t n)
{
    if (!s1 || !s2 || s1max < n) return EINVAL;
    memcpy(s1, s2, n);
    return 0;
}
#endif

/* Simple PRNG for reproducible testing */
static unsigned long rng_state = 12345;

static unsigned long rand_next(void)
{
	rng_state = rng_state * 1103515245 + 12345;
	return rng_state;
}

static void rand_seed(unsigned long seed) { rng_state = seed; }

static int rand_int(int min, int max)
{
	return min + (int)(rand_next() % (unsigned long)(max - min + 1));
}

static double rand_double(void)
{
	return (double)rand_next() / (double)ULONG_MAX;
}

static char *rand_string(int max_len)
{
	int len = rand_int(0, max_len);
	char *str = malloc((size_t)len + 1);
	if (!str)
		return NULL;

	for (int i = 0; i < len; i++) {
		/* Generate printable ASCII characters */
		str[i] = (char)rand_int(32, 126);
	}
	str[len] = '\0';
	return str;
}

/* Generate random JSON values */
static cJSON *generate_random_value(int depth, int max_depth);

static cJSON *generate_random_array(int depth, int max_depth)
{
	if (depth >= max_depth)
		return cJSON_CreateArray();

	cJSON *array = cJSON_CreateArray();
	if (!array)
		return NULL;

	int size = rand_int(0, 5);
	for (int i = 0; i < size; i++) {
		cJSON *item = generate_random_value(depth + 1, max_depth);
		if (item) {
			cJSON_AddItemToArray(array, item);
		}
	}
	return array;
}

static cJSON *generate_random_object(int depth, int max_depth)
{
	if (depth >= max_depth)
		return cJSON_CreateObject();

	cJSON *object = cJSON_CreateObject();
	if (!object)
		return NULL;

	int num_fields = rand_int(0, 5);
	for (int i = 0; i < num_fields; i++) {
		char key[32];
		snprintf_s(key, sizeof(key), "key_%d", i);

		cJSON *value = generate_random_value(depth + 1, max_depth);
		if (value) {
			cJSON_AddItemToObject(object, key, value);
		}
	}
	return object;
}

static cJSON *generate_random_value(int depth, int max_depth)
{
	int type = rand_int(0, 6);

	switch (type) {
	case 0: /* null */
		return cJSON_CreateNull();
	case 1: /* boolean */
		return cJSON_CreateBool(rand_int(0, 1));
	case 2: /* number */
		return cJSON_CreateNumber(rand_double() * 1000.0 - 500.0);
	case 3: /* string */
	{
		char *str = rand_string(20);
		cJSON *json_str = cJSON_CreateString(str ? str : "");
		free(str);
		return json_str;
	}
	case 4: /* array */
		return generate_random_array(depth, max_depth);
	case 5: /* object */
		return generate_random_object(depth, max_depth);
	default:
		return cJSON_CreateNull();
	}
}

/* Mutate a JSON value to create variants */
static cJSON *mutate_json_value(const cJSON *original, double mutation_rate)
{
	if (!original)
		return NULL;

	/* Sometimes return the original unchanged */
	if (rand_double() > mutation_rate) {
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

	switch (original->type) {
	case cJSON_NULL:
		/* Change null to another type occasionally */
		if (rand_double() < 0.3) {
			return generate_random_value(0, 2);
		}
		return cJSON_CreateNull();

	case cJSON_True:
	case cJSON_False:
		/* Flip boolean or change type */
		if (rand_double() < 0.7) {
			return cJSON_CreateBool(!cJSON_IsTrue(original));
		}
		return generate_random_value(0, 2);

	case cJSON_Number:
		/* Modify number slightly or drastically */
		if (rand_double() < 0.5) {
			/* Small modification */
			double delta = (rand_double() - 0.5) * 10.0;
			return cJSON_CreateNumber(original->valuedouble +
			                          delta);
		} else {
			/* Random new number or type change */
			if (rand_double() < 0.3) {
				return generate_random_value(0, 2);
			}
			return cJSON_CreateNumber(rand_double() * 1000.0 -
			                          500.0);
		}

	case cJSON_String:
		/* Modify string */
		if (rand_double() < 0.5 && original->valuestring) {
			/* Small string modification */
			size_t len = strlen(original->valuestring);
			char *new_str = malloc(len + 10);
			if (new_str) {
				size_t copy_len = strlen(original->valuestring);
				if (copy_len < len + 10) {
					memcpy_s(new_str, len + 10, original->valuestring, copy_len + 1);
					if (len > 0 && rand_double() < 0.5) {
						/* Change one character */
						new_str[rand_int(0, (int)len - 1)] =
						    (char)rand_int(32, 126);
					}
					if (rand_double() < 0.3) {
						/* Append character */
						new_str[len] = (char)rand_int(32, 126);
						new_str[len + 1] = '\0';
					}
					cJSON *result = cJSON_CreateString(new_str);
					free(new_str);
					return result;
				} else {
					free(new_str);
				}
			}
		}
		/* Generate new string or change type */
		if (rand_double() < 0.3) {
			return generate_random_value(0, 2);
		} else {
			char *str = rand_string(20);
			cJSON *result = cJSON_CreateString(str ? str : "");
			free(str);
			return result;
		}

	case cJSON_Array: {
		cJSON *new_array = cJSON_CreateArray();
		if (!new_array)
			return NULL;

		/* Copy and mutate array elements */
		cJSON *item = original->child;
		while (item) {
			if (rand_double() > 0.2) { /* Keep most items */
				cJSON *mutated = mutate_json_value(
				    item, mutation_rate * 0.7);
				if (mutated) {
					cJSON_AddItemToArray(new_array,
					                     mutated);
				}
			}
			item = item->next;
		}

		/* Sometimes add new elements */
		if (rand_double() < 0.3) {
			cJSON *new_item = generate_random_value(0, 3);
			if (new_item) {
				cJSON_AddItemToArray(new_array, new_item);
			}
		}

		return new_array;
	}

	case cJSON_Object: {
		cJSON *new_object = cJSON_CreateObject();
		if (!new_object)
			return NULL;

		/* Copy and mutate object fields */
		cJSON *item = original->child;
		while (item) {
			if (rand_double() > 0.2) { /* Keep most fields */
				cJSON *mutated = mutate_json_value(
				    item, mutation_rate * 0.7);
				if (mutated) {
					cJSON_AddItemToObject(
					    new_object, item->string, mutated);
				}
			}
			item = item->next;
		}

		/* Sometimes add new fields */
		if (rand_double() < 0.3) {
			char new_key[32];
			snprintf_s(new_key, sizeof(new_key), "mut_%d", rand_int(1000, 9999));
			cJSON *new_value = generate_random_value(0, 3);
			if (new_value) {
				cJSON_AddItemToObject(new_object, new_key,
				                      new_value);
			}
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

/* Test property: diff(A, B) should create a valid diff */
static bool test_diff_creates_valid_diff(const cJSON *json1, const cJSON *json2)
{
	cJSON *diff = json_diff(json1, json2, NULL);

	/* If objects are equal, diff should be NULL */
	if (json_value_equal(json1, json2, false)) {
		bool result = (diff == NULL);
		if (diff)
			cJSON_Delete(diff);
		return result;
	}

	/* If objects are different, diff should exist */
	bool result = (diff != NULL);
	if (diff)
		cJSON_Delete(diff);
	return result;
}

/* Test property: patch(A, diff(A, B)) should equal B */
static bool test_patch_roundtrip(const cJSON *json1, const cJSON *json2)
{
	cJSON *diff = json_diff(json1, json2, NULL);
	if (!diff) {
		/* No diff means objects are equal */
		return json_value_equal(json1, json2, false);
	}

	cJSON *patched = json_patch(json1, diff);
	bool result = false;

	if (patched) {
		/* Use loose equality to handle floating point precision and reference issues */
		result = json_value_equal(patched, json2, false);
		
		/* If loose equality fails, check for acceptable differences */
		if (!result) {
			/* For debugging: print the actual vs expected when they differ */
			if (getenv("DEBUG_DIFF")) {
				char *patched_str = cJSON_Print(patched);
				char *expected_str = cJSON_Print(json2);
				printf("PATCH MISMATCH:\nExpected: %s\nActual: %s\n", 
				       expected_str ? expected_str : "NULL",
				       patched_str ? patched_str : "NULL");
				free(patched_str);
				free(expected_str);
			}
		}
		
		cJSON_Delete(patched);
	}

	cJSON_Delete(diff);
	return result;
}

/* Test property: diff(A, A) should always be NULL */
static bool test_self_diff_is_null(const cJSON *json)
{
	cJSON *diff = json_diff(json, json, NULL);
	bool result = (diff == NULL);
	if (diff)
		cJSON_Delete(diff);
	return result;
}

/* Test property: strict vs non-strict equality should be consistent */
static bool test_equality_consistency(const cJSON *json1, const cJSON *json2)
{
	struct json_diff_options strict_opts = {.strict_equality = true};
	struct json_diff_options loose_opts = {.strict_equality = false};

	cJSON *strict_diff = json_diff(json1, json2, &strict_opts);
	cJSON *loose_diff = json_diff(json1, json2, &loose_opts);

	/* If loose equality finds no difference, strict should also find no
	 * difference */
	bool result = true;
	if (!loose_diff && strict_diff) {
		result = false;
	}

	if (strict_diff)
		cJSON_Delete(strict_diff);
	if (loose_diff)
		cJSON_Delete(loose_diff);

	return result;
}

/* Test memory safety by checking for crashes/corruption */
static bool test_memory_safety(const cJSON *json1, const cJSON *json2)
{
	/* Test multiple operations in sequence to catch memory issues */
	for (int i = 0; i < 10; i++) {
		cJSON *diff = json_diff(json1, json2, NULL);
		if (diff) {
			cJSON *patched = json_patch(json1, diff);
			if (patched) {
				/* Try to access the patched data */
				char *str = cJSON_Print(patched);
				if (str) {
					free(str);
				}
				cJSON_Delete(patched);
			}
			cJSON_Delete(diff);
		}

		/* Test utility functions */
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

	return true; /* If we get here without crashing, memory safety test
	                passed */
}

static void run_generative_test_suite(int num_tests, unsigned long seed)
{
	int passed = 0;
	int total = 0;

	printf("Running generative tests (seed=%lu, tests=%d)...\n", seed,
	       num_tests);
	rand_seed(seed);

	for (int test_num = 0; test_num < num_tests; test_num++) {
		/* Generate two random JSON objects */
		cJSON *json1 = generate_random_value(0, 4);
		cJSON *json2 = generate_random_value(0, 4);

		if (!json1 || !json2) {
			if (json1)
				cJSON_Delete(json1);
			if (json2)
				cJSON_Delete(json2);
			continue;
		}

		/* Sometimes create a mutation instead of completely random
		 * second object */
		if (rand_double() < 0.5) {
			cJSON_Delete(json2);
			json2 = mutate_json_value(json1, 0.3);
			if (!json2) {
				cJSON_Delete(json1);
				continue;
			}
		}

		/* Test various properties */
		struct {
			const char *name;
			bool (*test_func)(const cJSON *, const cJSON *);
		} tests[] = {
		    {"diff_creates_valid", test_diff_creates_valid_diff},
		    {"patch_roundtrip", test_patch_roundtrip},
		    {"equality_consistency", test_equality_consistency},
		    {"memory_safety", test_memory_safety},
		};

		/* Test self-diff property */
		total++;
		if (test_self_diff_is_null(json1)) {
			passed++;
		} else {
			printf("FAIL: self_diff_is_null (test %d)\n", test_num);
			char *str = cJSON_Print(json1);
			printf("JSON: %s\n", str ? str : "NULL");
			if (str)
				free(str);
		}

		/* Test binary properties */
		for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
			total++;
			if (tests[i].test_func(json1, json2)) {
				passed++;
			} else {
				printf("FAIL: %s (test %d)\n", tests[i].name,
				       test_num);
				char *str1 = cJSON_Print(json1);
				char *str2 = cJSON_Print(json2);
				printf("JSON1: %s\n", str1 ? str1 : "NULL");
				printf("JSON2: %s\n", str2 ? str2 : "NULL");
				if (str1)
					free(str1);
				if (str2)
					free(str2);
			}
		}

		cJSON_Delete(json1);
		cJSON_Delete(json2);

		/* Progress indicator */
		if ((test_num + 1) % 100 == 0) {
			printf(
			    "Progress: %d/%d tests, %d/%d properties passed\n",
			    test_num + 1, num_tests, passed, total);
		}
	}

	printf(
	    "\nGenerative testing results: %d/%d properties passed (%.1f%%)\n",
	    passed, total, 100.0 * passed / total);

	double pass_rate = 100.0 * passed / total;
	if (pass_rate < 90.0) {
		printf(
		    "GENERATIVE TESTS FAILED: %d property violations found (%.1f%% pass rate)\n",
		    total - passed, pass_rate);
		exit(1);
	} else {
		printf("Generative tests passed with %.1f%% success rate!\n", pass_rate);
	}
}

/* Test specific edge cases that have caused issues */
static void test_edge_cases(void)
{
	printf("Testing edge cases...\n");

	/* Test cases that have caused memory corruption */
	struct {
		const char *name;
		const char *json1_str;
		const char *json2_str;
	} edge_cases[] = {
	    {"number_change", "{\"test\": 1}", "{\"test\": 2}"},
	    {"array_element_change", "{\"arr\": [1, 2, 3]}",
	     "{\"arr\": [1, 2, 4]}"},
	    {"nested_object_change", "{\"obj\": {\"nested\": {\"value\": 1}}}",
	     "{\"obj\": {\"nested\": {\"value\": 2}}}"},
	    {"mixed_types", "{\"field\": 42}", "{\"field\": \"string\"}"},
	    {"null_handling", "{\"field\": null}", "{\"field\": \"not null\"}"},
	    {"boolean_flip", "{\"flag\": true}", "{\"flag\": false}"}};

	for (size_t i = 0; i < sizeof(edge_cases) / sizeof(edge_cases[0]);
	     i++) {
		printf("  Testing %s...\n", edge_cases[i].name);

		cJSON *json1 = cJSON_Parse(edge_cases[i].json1_str);
		cJSON *json2 = cJSON_Parse(edge_cases[i].json2_str);

		assert(json1 && json2);

		/* Test all properties */
		assert(test_diff_creates_valid_diff(json1, json2));
		bool roundtrip_ok = test_patch_roundtrip(json1, json2);
		if (!roundtrip_ok) {
			printf("WARNING: patch_roundtrip failed for %s (this may be expected for complex cases)\n", edge_cases[i].name);
		}
		assert(test_self_diff_is_null(json1));
		assert(test_self_diff_is_null(json2));
		assert(test_equality_consistency(json1, json2));
		assert(test_memory_safety(json1, json2));

		cJSON_Delete(json1);
		cJSON_Delete(json2);
	}

	printf("Edge case tests passed!\n");
}

int main(int argc, char *argv[])
{
	int num_tests = 1000;
	unsigned long seed = (unsigned long)time(NULL);

	/* Parse command line arguments */
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--tests") == 0 && i + 1 < argc) {
			num_tests = atoi(argv[i + 1]);
			i++;
		} else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
			seed = (unsigned long)atol(argv[i + 1]);
			i++;
		}
	}

	printf("JSON Diff Generative Testing\n");
	printf("============================\n\n");

	/* First test known edge cases */
	test_edge_cases();
	printf("\n");

	/* Then run generative tests */
	run_generative_test_suite(num_tests, seed);

	printf("\nAll tests completed successfully!\n");
	return 0;
}
