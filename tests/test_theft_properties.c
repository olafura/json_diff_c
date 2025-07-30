// SPDX-License-Identifier: Apache-2.0
#define __STDC_WANT_LIB_EXT1__ 1
#include "src/json_diff.h"
#include "vendor/theft/inc/theft.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Property-based testing for JSON Diff library using theft
 *
 * This replaces the existing generative and fuzzing tests with
 * systematic property-based testing that automatically generates
 * test cases and shrinks failing examples to minimal cases.
 */

/* Maximum depth for generated JSON structures */
#define MAX_JSON_GEN_DEPTH 8
#define MAX_ARRAY_SIZE 20
#define MAX_OBJECT_FIELDS 15
#define MAX_STRING_LENGTH 100

/* JSON generation state */
struct json_gen_env {
	int max_depth;
	int current_depth;
};

/* Forward declarations */
static enum theft_alloc_res json_alloc(struct theft *t, void *env,
                                       void **output);
static void json_free(void *instance, void *env);
static theft_hash json_hash(const void *instance, void *env);
static enum theft_shrink_res json_shrink(struct theft *t, const void *instance,
                                         uint32_t tactic, void *env,
                                         void **output);
static void json_print(FILE *f, const void *instance, void *env);

/* Type info for cJSON generation */
static struct theft_type_info json_type_info = {
    .alloc = json_alloc,
    .free = json_free,
    .hash = json_hash,
    .shrink = json_shrink,
    .print = json_print,
};

/**
 * Generate a random cJSON value based on theft's random number generator
 */
static cJSON *generate_json_value(struct theft *t, int depth, int max_depth)
{
	if (depth >= max_depth) {
		/* At max depth, only generate simple values */
		switch (theft_random_choice(t, 4)) {
		case 0:
			return cJSON_CreateNull();
		case 1:
			return cJSON_CreateBool(theft_random_choice(t, 2));
		case 2: {
			/* Generate number in reasonable range */
			double val =
			    (double)(theft_random_bits(t, 32)) - (1ULL << 31);
			val /= 1000.0; /* Scale down to avoid overflow issues */
			return cJSON_CreateNumber(val);
		}
		case 3: {
			/* Generate short string */
			uint32_t len = theft_random_choice(t, 20);
			char *str = malloc(len + 1);
			if (!str)
				return cJSON_CreateString("");

			for (uint32_t i = 0; i < len; i++) {
				/* Printable ASCII range */
				str[i] =
				    (char)(32 + (theft_random_bits(t, 6) % 95));
			}
			str[len] = '\0';
			cJSON *result = cJSON_CreateString(str);
			free(str);
			return result;
		}
		default:
			return cJSON_CreateNull();
		}
	}

	/* Choose type to generate */
	switch (theft_random_choice(t, 6)) {
	case 0:
		return cJSON_CreateNull();
	case 1:
		return cJSON_CreateBool(theft_random_choice(t, 2));
	case 2: {
		double val = (double)(theft_random_bits(t, 32)) - (1ULL << 31);
		val /= 1000.0;
		return cJSON_CreateNumber(val);
	}
	case 3: {
		uint32_t len = theft_random_choice(t, MAX_STRING_LENGTH);
		char *str = malloc(len + 1);
		if (!str)
			return cJSON_CreateString("");

		for (uint32_t i = 0; i < len; i++) {
			str[i] = (char)(32 + (theft_random_bits(t, 6) % 95));
		}
		str[len] = '\0';
		cJSON *result = cJSON_CreateString(str);
		free(str);
		return result;
	}
	case 4: {
		/* Generate array */
		cJSON *array = cJSON_CreateArray();
		if (!array)
			return cJSON_CreateNull();

		uint32_t size = theft_random_choice(t, MAX_ARRAY_SIZE);
		for (uint32_t i = 0; i < size; i++) {
			cJSON *item =
			    generate_json_value(t, depth + 1, max_depth);
			if (item) {
				cJSON_AddItemToArray(array, item);
			}
		}
		return array;
	}
	case 5: {
		/* Generate object */
		cJSON *object = cJSON_CreateObject();
		if (!object)
			return cJSON_CreateNull();

		uint32_t fields = theft_random_choice(t, MAX_OBJECT_FIELDS);
		for (uint32_t i = 0; i < fields; i++) {
			/* Generate key */
			char key[32];
			snprintf(key, sizeof(key), "key_%u", i);

			cJSON *value =
			    generate_json_value(t, depth + 1, max_depth);
			if (value) {
				cJSON_AddItemToObject(object, key, value);
			}
		}
		return object;
	}
	default:
		return cJSON_CreateNull();
	}
}

/**
 * Allocate callback for theft - generates a random cJSON structure
 */
static enum theft_alloc_res json_alloc(struct theft *t, void *env,
                                       void **output)
{
	struct json_gen_env *gen_env = (struct json_gen_env *)env;
	if (!gen_env) {
		gen_env = &(struct json_gen_env){
		    .max_depth = MAX_JSON_GEN_DEPTH, .current_depth = 0};
	}

	cJSON *json = generate_json_value(t, 0, gen_env->max_depth);
	if (!json) {
		return THEFT_ALLOC_ERROR;
	}

	*output = json;
	return THEFT_ALLOC_OK;
}

/**
 * Free callback for theft - frees cJSON structure
 */
static void json_free(void *instance, void *env)
{
	(void)env;
	if (instance) {
		cJSON_Delete((cJSON *)instance);
	}
}

/**
 * Hash callback for theft - creates hash of JSON structure
 */
static theft_hash json_hash(const void *instance, void *env)
{
	(void)env;
	if (!instance)
		return 0;

	char *json_str = cJSON_PrintUnformatted((const cJSON *)instance);
	if (!json_str)
		return 0;

	/* Simple hash function */
	theft_hash hash = 0;
	const char *str = json_str;
	while (*str) {
		hash = hash * 31 + (unsigned char)*str;
		str++;
	}

	free(json_str);
	return hash;
}

/**
 * Print callback for theft - prints JSON for debugging
 */
static void json_print(FILE *f, const void *instance, void *env)
{
	(void)env;
	if (!f || !instance)
		return;

	char *json_str = cJSON_Print((const cJSON *)instance);
	if (json_str) {
		fprintf(f, "%s", json_str);
		free(json_str);
	} else {
		fprintf(f, "<NULL JSON>");
	}
}

/**
 * Shrink callback for theft - creates simpler versions of JSON
 */
static enum theft_shrink_res json_shrink(struct theft *t, const void *instance,
                                         uint32_t tactic, void *env,
                                         void **output)
{
	(void)t;
	(void)env;
	if (!instance)
		return THEFT_SHRINK_NO_MORE_TACTICS;

	const cJSON *json = (const cJSON *)instance;

	switch (tactic) {
	case 0:
		/* Try to replace with null */
		if (!cJSON_IsNull(json)) {
			*output = cJSON_CreateNull();
			return *output ? THEFT_SHRINK_OK : THEFT_SHRINK_ERROR;
		}
		return THEFT_SHRINK_DEAD_END;

	case 1:
		/* Try to replace with empty object/array */
		if (cJSON_IsObject(json) && cJSON_GetArraySize(json) > 0) {
			*output = cJSON_CreateObject();
			return *output ? THEFT_SHRINK_OK : THEFT_SHRINK_ERROR;
		}
		if (cJSON_IsArray(json) && cJSON_GetArraySize(json) > 0) {
			*output = cJSON_CreateArray();
			return *output ? THEFT_SHRINK_OK : THEFT_SHRINK_ERROR;
		}
		return THEFT_SHRINK_DEAD_END;

	case 2:
		/* Try to shrink array by removing first element */
		if (cJSON_IsArray(json)) {
			int size = cJSON_GetArraySize(json);
			if (size > 0) {
				*output = cJSON_CreateArray();
				if (!*output)
					return THEFT_SHRINK_ERROR;

				for (int i = 1; i < size; i++) {
					cJSON *item =
					    cJSON_GetArrayItem(json, i);
					if (item) {
						cJSON *copy =
						    cJSON_Duplicate(item, 1);
						if (copy) {
							cJSON_AddItemToArray(
							    (cJSON *)*output,
							    copy);
						}
					}
				}
				return THEFT_SHRINK_OK;
			}
		}
		return THEFT_SHRINK_DEAD_END;

	case 3:
		/* Try to shrink object by removing first field */
		if (cJSON_IsObject(json)) {
			cJSON *first = json->child;
			if (first && first->next) {
				*output = cJSON_CreateObject();
				if (!*output)
					return THEFT_SHRINK_ERROR;

				cJSON *item = first->next;
				while (item) {
					cJSON *copy = cJSON_Duplicate(item, 1);
					if (copy) {
						cJSON_AddItemToObject(
						    (cJSON *)*output,
						    item->string, copy);
					}
					item = item->next;
				}
				return THEFT_SHRINK_OK;
			}
		}
		return THEFT_SHRINK_DEAD_END;

	case 4:
		/* Try to shrink string */
		if (cJSON_IsString(json) && json->valuestring) {
			size_t len = strlen(json->valuestring);
			if (len > 0) {
				char *shorter = malloc(len);
				if (!shorter)
					return THEFT_SHRINK_ERROR;
				strncpy(shorter, json->valuestring, len - 1);
				shorter[len - 1] = '\0';
				*output = cJSON_CreateString(shorter);
				free(shorter);
				return *output ? THEFT_SHRINK_OK
				               : THEFT_SHRINK_ERROR;
			}
		}
		return THEFT_SHRINK_DEAD_END;

	default:
		return THEFT_SHRINK_NO_MORE_TACTICS;
	}
}

/**
 * PROPERTY 1: Self-diff should always be NULL
 * For any JSON value j, diff(j, j) should return NULL
 */
static enum theft_trial_res prop_self_diff_is_null(struct theft *t, void *arg1)
{
	(void)t;
	cJSON *json = (cJSON *)arg1;

	cJSON *diff = json_diff(json, json, NULL);

	if (diff != NULL) {
		cJSON_Delete(diff);
		return THEFT_TRIAL_FAIL;
	}

	return THEFT_TRIAL_PASS;
}

/**
 * PROPERTY 2: Diff-patch roundtrip
 * For any JSON values j1, j2: patch(j1, diff(j1, j2)) should equal j2
 */
static enum theft_trial_res prop_diff_patch_roundtrip(struct theft *t,
                                                      void *arg1, void *arg2)
{
	(void)t;
	cJSON *json1 = (cJSON *)arg1;
	cJSON *json2 = (cJSON *)arg2;

	cJSON *diff = json_diff(json1, json2, NULL);

	/* If no diff, values should be equal */
	if (!diff) {
		bool equal = json_value_equal(json1, json2, false);
		return equal ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
	}

	cJSON *patched = json_patch(json1, diff);
	cJSON_Delete(diff);

	if (!patched) {
		return THEFT_TRIAL_FAIL;
	}

	bool equal = json_value_equal(patched, json2, false);
	cJSON_Delete(patched);

	return equal ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/**
 * PROPERTY 3: Diff symmetry
 * diff(j1, j2) should exist if and only if diff(j2, j1) exists
 */
static enum theft_trial_res prop_diff_symmetry(struct theft *t, void *arg1,
                                               void *arg2)
{
	(void)t;
	cJSON *json1 = (cJSON *)arg1;
	cJSON *json2 = (cJSON *)arg2;

	cJSON *diff12 = json_diff(json1, json2, NULL);
	cJSON *diff21 = json_diff(json2, json1, NULL);

	bool both_null = (diff12 == NULL) && (diff21 == NULL);
	bool both_exist = (diff12 != NULL) && (diff21 != NULL);

	if (diff12)
		cJSON_Delete(diff12);
	if (diff21)
		cJSON_Delete(diff21);

	return (both_null || both_exist) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/**
 * PROPERTY 4: Value equality consistency
 * json_value_equal should be consistent with diff behavior
 */
static enum theft_trial_res prop_equality_consistency(struct theft *t,
                                                      void *arg1, void *arg2)
{
	(void)t;
	cJSON *json1 = (cJSON *)arg1;
	cJSON *json2 = (cJSON *)arg2;

	bool equal_strict = json_value_equal(json1, json2, true);
	bool equal_loose = json_value_equal(json1, json2, false);

	struct json_diff_options opts_strict = {.strict_equality = true};
	struct json_diff_options opts_loose = {.strict_equality = false};

	cJSON *diff_strict = json_diff(json1, json2, &opts_strict);
	cJSON *diff_loose = json_diff(json1, json2, &opts_loose);

	bool diff_strict_null = (diff_strict == NULL);
	bool diff_loose_null = (diff_loose == NULL);

	if (diff_strict)
		cJSON_Delete(diff_strict);
	if (diff_loose)
		cJSON_Delete(diff_loose);

	/* If values are equal, diff should be null */
	bool strict_consistent = (equal_strict == diff_strict_null);
	bool loose_consistent = (equal_loose == diff_loose_null);

	return (strict_consistent && loose_consistent) ? THEFT_TRIAL_PASS
	                                               : THEFT_TRIAL_FAIL;
}

/**
 * PROPERTY 5: No crashes or memory leaks
 * All operations should complete without crashing
 */
static enum theft_trial_res prop_no_crashes(struct theft *t, void *arg1,
                                            void *arg2)
{
	(void)t;
	cJSON *json1 = (cJSON *)arg1;
	cJSON *json2 = (cJSON *)arg2;

	/* Try various operations that shouldn't crash */
	cJSON *diff = json_diff(json1, json2, NULL);
	if (diff) {
		cJSON *patched1 = json_patch(json1, diff);
		cJSON *patched2 = json_patch(json2, diff);

		if (patched1)
			cJSON_Delete(patched1);
		if (patched2)
			cJSON_Delete(patched2);
		cJSON_Delete(diff);
	}

	/* Test with different options */
	struct json_diff_options opts = {.strict_equality = true};
	diff = json_diff(json1, json2, &opts);
	if (diff) {
		cJSON_Delete(diff);
	}

	/* Test value equality */
	(void)json_value_equal(json1, json2, true);
	(void)json_value_equal(json1, json2, false);

	return THEFT_TRIAL_PASS;
}

/**
 * Run all property-based tests
 */
bool run_theft_property_tests(void)
{
	printf("Running property-based tests with theft...\n");

	struct json_gen_env env = {.max_depth = MAX_JSON_GEN_DEPTH,
	                           .current_depth = 0};
	theft_seed seed = theft_seed_of_time();
	bool all_passed = true;

	/* Check if we're in quick test mode (for CI) */
	bool quick_test = getenv("THEFT_QUICK_TEST") != NULL;
	size_t base_trials = quick_test ? 100 : 1000; /* Reduce trials for CI */

	if (quick_test) {
		printf("Running in quick test mode (reduced trials for CI)\n");
	}

	/* Property 1: Self-diff is null */
	{
		json_type_info.env = &env;
		struct theft_run_config config = {
		    .name = "prop_self_diff_is_null",
		    .prop1 = prop_self_diff_is_null,
		    .type_info = {&json_type_info},
		    .trials = base_trials,
		    .seed = seed,
		};

		enum theft_run_res res = theft_run(&config);
		printf("Property 1 (self-diff is null): %s\n",
		       res == THEFT_RUN_PASS ? "PASS" : "FAIL");
		if (res != THEFT_RUN_PASS)
			all_passed = false;
	}

	/* Property 2: Diff-patch roundtrip */
	{
		struct theft_run_config config = {
		    .name = "prop_diff_patch_roundtrip",
		    .prop2 = prop_diff_patch_roundtrip,
		    .type_info = {&json_type_info, &json_type_info},
		    .trials = base_trials,
		    .seed = seed + 1,
		};

		enum theft_run_res res = theft_run(&config);
		printf("Property 2 (diff-patch roundtrip): %s\n",
		       res == THEFT_RUN_PASS ? "PASS" : "FAIL");
		if (res != THEFT_RUN_PASS)
			all_passed = false;
	}

	/* Property 3: Diff symmetry */
	{
		struct theft_run_config config = {
		    .name = "prop_diff_symmetry",
		    .prop2 = prop_diff_symmetry,
		    .type_info = {&json_type_info, &json_type_info},
		    .trials = base_trials,
		    .seed = seed + 2,
		};

		enum theft_run_res res = theft_run(&config);
		printf("Property 3 (diff symmetry): %s\n",
		       res == THEFT_RUN_PASS ? "PASS" : "FAIL");
		if (res != THEFT_RUN_PASS)
			all_passed = false;
	}

	/* Property 4: Equality consistency */
	{
		struct theft_run_config config = {
		    .name = "prop_equality_consistency",
		    .prop2 = prop_equality_consistency,
		    .type_info = {&json_type_info, &json_type_info},
		    .trials = base_trials,
		    .seed = seed + 3,
		};

		enum theft_run_res res = theft_run(&config);
		printf("Property 4 (equality consistency): %s\n",
		       res == THEFT_RUN_PASS ? "PASS" : "FAIL");
		if (res != THEFT_RUN_PASS)
			all_passed = false;
	}

	/* Property 5: No crashes */
	{
		struct theft_run_config config = {
		    .name = "prop_no_crashes",
		    .prop2 = prop_no_crashes,
		    .type_info = {&json_type_info, &json_type_info},
		    .trials = base_trials * 2,
		    .seed = seed + 4,
		};

		enum theft_run_res res = theft_run(&config);
		printf("Property 5 (no crashes): %s\n",
		       res == THEFT_RUN_PASS ? "PASS" : "FAIL");
		if (res != THEFT_RUN_PASS)
			all_passed = false;
	}

	return all_passed;
}

int main(void)
{
	printf("JSON Diff Property-Based Test Suite (using theft)\n");
	printf("================================================\n\n");

	bool success = run_theft_property_tests();

	printf("\n================================================\n");
	if (success) {
		printf("All property-based tests PASSED! ✓\n");
		return 0;
	} else {
		printf("Some property-based tests FAILED! ✗\n");
		return 1;
	}
}
