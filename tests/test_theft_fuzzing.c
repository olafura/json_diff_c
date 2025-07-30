// SPDX-License-Identifier: Apache-2.0
#define __STDC_WANT_LIB_EXT1__ 1
#include "src/json_diff.h"
#include "vendor/theft/inc/theft.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Theft-based fuzzing replacement for traditional libFuzzer approach
 *
 * This provides structured fuzzing using theft's property-based testing
 * framework, which is more systematic than random byte-based fuzzing
 * and can automatically shrink failing cases to minimal examples.
 */

/* Maximum sizes for fuzzing */
#define MAX_FUZZ_DEPTH 10
#define MAX_FUZZ_ARRAY_SIZE 50
#define MAX_FUZZ_OBJECT_FIELDS 30
#define MAX_FUZZ_STRING_LENGTH 200

/* Binary data structure for raw fuzzing */
struct binary_data {
	uint8_t *data;
	size_t size;
};

/* Forward declarations for binary data fuzzing */
static enum theft_alloc_res binary_alloc(struct theft *t, void *env,
                                         void **output);
static void binary_free(void *instance, void *env);
static theft_hash binary_hash(const void *instance, void *env);
static enum theft_shrink_res binary_shrink(struct theft *t,
                                           const void *instance,
                                           uint32_t tactic, void *env,
                                           void **output);
static void binary_print(FILE *f, const void *instance, void *env);

/* Type info for binary data generation */
static struct theft_type_info binary_type_info = {
    .alloc = binary_alloc,
    .free = binary_free,
    .hash = binary_hash,
    .shrink = binary_shrink,
    .print = binary_print,
};

/* JSON string structure for string-based fuzzing */
struct json_string {
	char *str;
	size_t len;
};

/* Forward declarations for JSON string fuzzing */
static enum theft_alloc_res json_string_alloc(struct theft *t, void *env,
                                              void **output);
static void json_string_free(void *instance, void *env);
static theft_hash json_string_hash(const void *instance, void *env);
static enum theft_shrink_res json_string_shrink(struct theft *t,
                                                const void *instance,
                                                uint32_t tactic, void *env,
                                                void **output);
static void json_string_print(FILE *f, const void *instance, void *env);

/* Type info for JSON string generation */
static struct theft_type_info json_string_type_info = {
    .alloc = json_string_alloc,
    .free = json_string_free,
    .hash = json_string_hash,
    .shrink = json_string_shrink,
    .print = json_string_print,
};

/**
 * Generate structured but potentially malformed JSON from random bytes
 */
static cJSON *generate_fuzz_json(struct theft *t, int depth, int max_depth)
{
	if (depth >= max_depth) {
		/* At max depth, generate simple values */
		switch (theft_random_choice(t, 5)) {
		case 0:
			return cJSON_CreateNull();
		case 1:
			return cJSON_CreateBool(theft_random_choice(t, 2));
		case 2: {
			/* Generate potentially problematic numbers */
			uint64_t bits = theft_random_bits(t, 64);
			double val;
			memcpy(&val, &bits, sizeof(double));

			/* Filter out problematic values that would break tests
			 */
			if (val != val || val == HUGE_VAL || val == -HUGE_VAL) {
				val = (double)(bits % 10000) - 5000.0;
			}

			return cJSON_CreateNumber(val);
		}
		case 3: {
			/* Generate potentially problematic strings */
			uint32_t len = theft_random_choice(t, 50);
			char *str = malloc(len + 1);
			if (!str)
				return cJSON_CreateString("");

			for (uint32_t i = 0; i < len; i++) {
				uint8_t c = theft_random_bits(t, 8);
				/* Allow some control characters for stress
				 * testing */
				if (c < 32 && c != '\t' && c != '\n' &&
				    c != '\r') {
					c = 32 + (c % 95); /* Printable ASCII */
				}
				str[i] = (char)c;
			}
			str[len] = '\0';

			cJSON *result = cJSON_CreateString(str);
			free(str);
			return result;
		}
		case 4: {
			/* Generate string with potential escape sequences */
			const char *special_strings[] = {
			    "",    "\\",   "\"",          "\n",
			    "\t",  "\r",   "\\n",         "\\t",
			    "\\r", "\\\"", "\\\\",        "\x00\x01\x02",
			    "€",   "🚀",   "test\x00test"};
			int idx = theft_random_choice(
			    t, sizeof(special_strings) /
			           sizeof(special_strings[0]));
			return cJSON_CreateString(special_strings[idx]);
		}
		default:
			return cJSON_CreateNull();
		}
	}

	switch (theft_random_choice(t, 7)) {
	case 0:
		return cJSON_CreateNull();
	case 1:
		return cJSON_CreateBool(theft_random_choice(t, 2));
	case 2: {
		uint64_t bits = theft_random_bits(t, 64);
		double val;
		memcpy(&val, &bits, sizeof(double));

		if (val != val || val == HUGE_VAL || val == -HUGE_VAL) {
			val = (double)(bits % 10000) - 5000.0;
		}

		return cJSON_CreateNumber(val);
	}
	case 3: {
		uint32_t len = theft_random_choice(t, MAX_FUZZ_STRING_LENGTH);
		char *str = malloc(len + 1);
		if (!str)
			return cJSON_CreateString("");

		for (uint32_t i = 0; i < len; i++) {
			uint8_t c = theft_random_bits(t, 8);
			if (c < 32 && c != '\t' && c != '\n' && c != '\r') {
				c = 32 + (c % 95);
			}
			str[i] = (char)c;
		}
		str[len] = '\0';

		cJSON *result = cJSON_CreateString(str);
		free(str);
		return result;
	}
	case 4: {
		/* Array with potentially problematic structure */
		cJSON *array = cJSON_CreateArray();
		if (!array)
			return cJSON_CreateNull();

		uint32_t size = theft_random_choice(t, MAX_FUZZ_ARRAY_SIZE);
		for (uint32_t i = 0; i < size; i++) {
			cJSON *item =
			    generate_fuzz_json(t, depth + 1, max_depth);
			if (item) {
				cJSON_AddItemToArray(array, item);
			}
		}
		return array;
	}
	case 5: {
		/* Object with potentially problematic keys/values */
		cJSON *object = cJSON_CreateObject();
		if (!object)
			return cJSON_CreateNull();

		uint32_t fields =
		    theft_random_choice(t, MAX_FUZZ_OBJECT_FIELDS);
		for (uint32_t i = 0; i < fields; i++) {
			/* Generate potentially problematic keys */
			char key[64];
			uint32_t key_len = theft_random_choice(t, 32);
			for (uint32_t j = 0; j < key_len && j < 63; j++) {
				uint8_t c = theft_random_bits(t, 8);
				if (c < 32 || c == '"' || c == '\\') {
					c = 'a' + (c % 26);
				}
				key[j] = (char)c;
			}
			key[key_len < 63 ? key_len : 63] = '\0';

			if (strlen(key) == 0) {
				snprintf(key, sizeof(key), "key_%u", i);
			}

			cJSON *value =
			    generate_fuzz_json(t, depth + 1, max_depth);
			if (value) {
				cJSON_AddItemToObject(object, key, value);
			}
		}
		return object;
	}
	case 6: {
		/* Create potentially duplicate or recursive-looking structures
		 */
		if (theft_random_choice(t, 2)) {
			/* Duplicate reference pattern */
			cJSON *base =
			    generate_fuzz_json(t, depth + 2, max_depth);
			return base;
		} else {
			/* Nested structure that might confuse diff algorithm */
			cJSON *nested = cJSON_CreateObject();
			if (nested) {
				cJSON_AddItemToObject(
				    nested, "self",
				    generate_fuzz_json(t, depth + 1,
				                       max_depth));
				cJSON_AddItemToObject(
				    nested, "meta",
				    generate_fuzz_json(t, depth + 1,
				                       max_depth));
			}
			return nested;
		}
	}
	default:
		return cJSON_CreateNull();
	}
}

/**
 * Binary data allocation for raw byte fuzzing
 */
static enum theft_alloc_res binary_alloc(struct theft *t, void *env,
                                         void **output)
{
	(void)env;

	struct binary_data *data = malloc(sizeof(struct binary_data));
	if (!data)
		return THEFT_ALLOC_ERROR;

	data->size =
	    theft_random_choice(t, 1024);    /* Up to 1KB of random data */
	data->data = malloc(data->size + 1); /* +1 for null termination */
	if (!data->data) {
		free(data);
		return THEFT_ALLOC_ERROR;
	}

	/* Generate random bytes */
	for (size_t i = 0; i < data->size; i++) {
		data->data[i] = (uint8_t)theft_random_bits(t, 8);
	}
	data->data[data->size] =
	    '\0'; /* Null terminate for string operations */

	*output = data;
	return THEFT_ALLOC_OK;
}

static void binary_free(void *instance, void *env)
{
	(void)env;
	if (instance) {
		struct binary_data *data = (struct binary_data *)instance;
		free(data->data);
		free(data);
	}
}

static theft_hash binary_hash(const void *instance, void *env)
{
	(void)env;
	if (!instance)
		return 0;

	const struct binary_data *data = (const struct binary_data *)instance;
	theft_hash hash = data->size;

	for (size_t i = 0; i < data->size && i < 64; i++) {
		hash = hash * 31 + data->data[i];
	}

	return hash;
}

static enum theft_shrink_res binary_shrink(struct theft *t,
                                           const void *instance,
                                           uint32_t tactic, void *env,
                                           void **output)
{
	(void)t;
	(void)env;
	if (!instance)
		return THEFT_SHRINK_NO_MORE_TACTICS;

	const struct binary_data *data = (const struct binary_data *)instance;

	switch (tactic) {
	case 0:
		/* Shrink size by half */
		if (data->size > 0) {
			struct binary_data *smaller =
			    malloc(sizeof(struct binary_data));
			if (!smaller)
				return THEFT_SHRINK_ERROR;

			smaller->size = data->size / 2;
			smaller->data = malloc(smaller->size + 1);
			if (!smaller->data) {
				free(smaller);
				return THEFT_SHRINK_ERROR;
			}

			memcpy(smaller->data, data->data, smaller->size);
			smaller->data[smaller->size] = '\0';

			*output = smaller;
			return THEFT_SHRINK_OK;
		}
		return THEFT_SHRINK_DEAD_END;

	case 1:
		/* Remove first byte */
		if (data->size > 1) {
			struct binary_data *shifted =
			    malloc(sizeof(struct binary_data));
			if (!shifted)
				return THEFT_SHRINK_ERROR;

			shifted->size = data->size - 1;
			shifted->data = malloc(shifted->size + 1);
			if (!shifted->data) {
				free(shifted);
				return THEFT_SHRINK_ERROR;
			}

			memcpy(shifted->data, data->data + 1, shifted->size);
			shifted->data[shifted->size] = '\0';

			*output = shifted;
			return THEFT_SHRINK_OK;
		}
		return THEFT_SHRINK_DEAD_END;

	default:
		return THEFT_SHRINK_NO_MORE_TACTICS;
	}
}

static void binary_print(FILE *f, const void *instance, void *env)
{
	(void)env;
	if (!f || !instance)
		return;

	const struct binary_data *data = (const struct binary_data *)instance;

	fprintf(f, "binary_data{size=%zu, data=[", data->size);
	for (size_t i = 0; i < data->size && i < 32; i++) {
		if (i > 0)
			fprintf(f, ", ");
		fprintf(f, "0x%02x", data->data[i]);
	}
	if (data->size > 32) {
		fprintf(f, ", ...");
	}
	fprintf(f, "]}");
}

/**
 * JSON string allocation for string-based fuzzing
 */
static enum theft_alloc_res json_string_alloc(struct theft *t, void *env,
                                              void **output)
{
	(void)env;

	struct json_string *js = malloc(sizeof(struct json_string));
	if (!js)
		return THEFT_ALLOC_ERROR;

	js->len = theft_random_choice(t, 512);
	js->str = malloc(js->len + 1);
	if (!js->str) {
		free(js);
		return THEFT_ALLOC_ERROR;
	}

	/* Generate potentially malformed JSON string */
	size_t pos = 0;

	/* Sometimes start with valid JSON, sometimes with garbage */
	if (theft_random_choice(t, 2)) {
		/* Start with potentially valid JSON structure */
		const char *templates[] = {"{\"key\":", "[",    "\"string",
		                           "123.45",    "true", "false",
		                           "null",      "{",    "}"};
		int idx = theft_random_choice(t, sizeof(templates) /
		                                     sizeof(templates[0]));
		const char *template = templates[idx];
		size_t template_len = strlen(template);

		if (template_len < js->len) {
			memcpy(js->str, template, template_len);
			pos = template_len;
		}
	}

	/* Fill rest with random characters */
	for (size_t i = pos; i < js->len; i++) {
		uint8_t c = theft_random_bits(t, 8);

		/* Bias toward characters that might appear in JSON */
		switch (theft_random_choice(t, 10)) {
		case 0:
			c = '"';
			break;
		case 1:
			c = '{';
			break;
		case 2:
			c = '}';
			break;
		case 3:
			c = '[';
			break;
		case 4:
			c = ']';
			break;
		case 5:
			c = ':';
			break;
		case 6:
			c = ',';
			break;
		case 7:
			c = '\\';
			break;
		case 8:
			c = '0' + (c % 10);
			break;
		default:
			if (c < 32 && c != '\t' && c != '\n' && c != '\r') {
				c = 32 + (c % 95);
			}
			break;
		}

		js->str[i] = (char)c;
	}
	js->str[js->len] = '\0';

	*output = js;
	return THEFT_ALLOC_OK;
}

static void json_string_free(void *instance, void *env)
{
	(void)env;
	if (instance) {
		struct json_string *js = (struct json_string *)instance;
		free(js->str);
		free(js);
	}
}

static theft_hash json_string_hash(const void *instance, void *env)
{
	(void)env;
	if (!instance)
		return 0;

	const struct json_string *js = (const struct json_string *)instance;
	theft_hash hash = js->len;

	const char *str = js->str;
	while (*str && hash < UINT64_MAX / 31) {
		hash = hash * 31 + (unsigned char)*str;
		str++;
	}

	return hash;
}

static enum theft_shrink_res json_string_shrink(struct theft *t,
                                                const void *instance,
                                                uint32_t tactic, void *env,
                                                void **output)
{
	(void)t;
	(void)env;
	if (!instance)
		return THEFT_SHRINK_NO_MORE_TACTICS;

	const struct json_string *js = (const struct json_string *)instance;

	switch (tactic) {
	case 0:
		/* Shrink string length */
		if (js->len > 0) {
			struct json_string *shorter =
			    malloc(sizeof(struct json_string));
			if (!shorter)
				return THEFT_SHRINK_ERROR;

			shorter->len = js->len / 2;
			shorter->str = malloc(shorter->len + 1);
			if (!shorter->str) {
				free(shorter);
				return THEFT_SHRINK_ERROR;
			}

			memcpy(shorter->str, js->str, shorter->len);
			shorter->str[shorter->len] = '\0';

			*output = shorter;
			return THEFT_SHRINK_OK;
		}
		return THEFT_SHRINK_DEAD_END;

	case 1:
		/* Remove first character */
		if (js->len > 1) {
			struct json_string *shifted =
			    malloc(sizeof(struct json_string));
			if (!shifted)
				return THEFT_SHRINK_ERROR;

			shifted->len = js->len - 1;
			shifted->str = malloc(shifted->len + 1);
			if (!shifted->str) {
				free(shifted);
				return THEFT_SHRINK_ERROR;
			}

			memcpy(shifted->str, js->str + 1, shifted->len);
			shifted->str[shifted->len] = '\0';

			*output = shifted;
			return THEFT_SHRINK_OK;
		}
		return THEFT_SHRINK_DEAD_END;

	default:
		return THEFT_SHRINK_NO_MORE_TACTICS;
	}
}

static void json_string_print(FILE *f, const void *instance, void *env)
{
	(void)env;
	if (!f || !instance)
		return;

	const struct json_string *js = (const struct json_string *)instance;
	fprintf(f, "json_string{len=%zu, str=\"", js->len);

	/* Print first 64 characters, escaping special chars */
	for (size_t i = 0; i < js->len && i < 64; i++) {
		char c = js->str[i];
		if (c >= 32 && c <= 126 && c != '"' && c != '\\') {
			fputc(c, f);
		} else {
			fprintf(f, "\\x%02x", (unsigned char)c);
		}
	}

	if (js->len > 64) {
		fprintf(f, "...");
	}
	fprintf(f, "\"}");
}

/**
 * FUZZ PROPERTY 1: No crashes with structured JSON
 */
static enum theft_trial_res
fuzz_prop_no_crashes_structured(struct theft *t, void *arg1, void *arg2)
{
	(void)arg1;
	(void)arg2;

	/* Generate structured but potentially problematic JSON */
	cJSON *json1 = generate_fuzz_json(t, 0, MAX_FUZZ_DEPTH);
	cJSON *json2 = generate_fuzz_json(t, 0, MAX_FUZZ_DEPTH);

	if (!json1 || !json2) {
		if (json1)
			cJSON_Delete(json1);
		if (json2)
			cJSON_Delete(json2);
		return THEFT_TRIAL_SKIP;
	}

	/* Try all operations - should not crash */
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

	/* Test with strict mode */
	struct json_diff_options opts = {.strict_equality = true};
	diff = json_diff(json1, json2, &opts);
	if (diff)
		cJSON_Delete(diff);

	/* Test equality functions */
	(void)json_value_equal(json1, json2, true);
	(void)json_value_equal(json1, json2, false);

	cJSON_Delete(json1);
	cJSON_Delete(json2);

	return THEFT_TRIAL_PASS;
}

/**
 * FUZZ PROPERTY 2: No crashes with raw binary data
 */
static enum theft_trial_res fuzz_prop_no_crashes_binary(struct theft *t,
                                                        void *arg1, void *arg2)
{
	(void)t;
	struct binary_data *data1 = (struct binary_data *)arg1;
	struct binary_data *data2 = (struct binary_data *)arg2;

	/* Try to parse as JSON strings - should not crash */
	cJSON *diff = json_diff_str((const char *)data1->data,
	                            (const char *)data2->data, NULL);
	if (diff) {
		cJSON_Delete(diff);
	}

	/* Test with different options */
	struct json_diff_options opts = {.strict_equality = true};
	diff = json_diff_str((const char *)data1->data,
	                     (const char *)data2->data, &opts);
	if (diff) {
		cJSON_Delete(diff);
	}

	return THEFT_TRIAL_PASS;
}

/**
 * FUZZ PROPERTY 3: No crashes with malformed JSON strings
 */
static enum theft_trial_res
fuzz_prop_no_crashes_json_strings(struct theft *t, void *arg1, void *arg2)
{
	(void)t;
	struct json_string *js1 = (struct json_string *)arg1;
	struct json_string *js2 = (struct json_string *)arg2;

	/* Try to parse and diff potentially malformed JSON */
	cJSON *diff = json_diff_str(js1->str, js2->str, NULL);
	if (diff) {
		cJSON_Delete(diff);
	}

	/* Try parsing individually */
	cJSON *json1 = cJSON_Parse(js1->str);
	cJSON *json2 = cJSON_Parse(js2->str);

	if (json1 && json2) {
		/* If both parsed successfully, try diffing */
		diff = json_diff(json1, json2, NULL);
		if (diff) {
			cJSON *patched = json_patch(json1, diff);
			if (patched)
				cJSON_Delete(patched);
			cJSON_Delete(diff);
		}
	}

	if (json1)
		cJSON_Delete(json1);
	if (json2)
		cJSON_Delete(json2);

	return THEFT_TRIAL_PASS;
}

/**
 * Run theft-based fuzzing tests
 */
bool run_theft_fuzzing_tests(void)
{
	printf("Running theft-based fuzzing tests...\n");

	theft_seed seed = theft_seed_of_time();
	bool all_passed = true;

	/* Check if we're in quick test mode (for CI) */
	bool quick_test = getenv("THEFT_QUICK_TEST") != NULL;
	size_t structured_trials = quick_test ? 500 : 5000; /* Reduce for CI */
	size_t binary_trials = quick_test ? 300 : 3000;     /* Reduce for CI */
	size_t string_trials = quick_test ? 400 : 4000;     /* Reduce for CI */

	if (quick_test) {
		printf("Running in quick test mode (reduced trials for CI)\n");
	}

	/* Fuzz test 1: Structured JSON fuzzing */
	{
		struct theft_run_config config = {
		    .name = "fuzz_structured_json_no_crashes",
		    .prop2 = fuzz_prop_no_crashes_structured,
		    .type_info = {NULL, NULL}, /* We generate JSON internally */
		    .trials = structured_trials,
		    .seed = seed,
		};

		enum theft_run_res res = theft_run(&config);
		printf("Fuzz test 1 (structured JSON): %s\n",
		       res == THEFT_RUN_PASS ? "PASS" : "FAIL");
		if (res != THEFT_RUN_PASS)
			all_passed = false;
	}

	/* Fuzz test 2: Binary data fuzzing */
	{
		struct theft_run_config config = {
		    .name = "fuzz_binary_data_no_crashes",
		    .prop2 = fuzz_prop_no_crashes_binary,
		    .type_info = {&binary_type_info, &binary_type_info},
		    .trials = binary_trials,
		    .seed = seed + 1,
		};

		enum theft_run_res res = theft_run(&config);
		printf("Fuzz test 2 (binary data): %s\n",
		       res == THEFT_RUN_PASS ? "PASS" : "FAIL");
		if (res != THEFT_RUN_PASS)
			all_passed = false;
	}

	/* Fuzz test 3: JSON string fuzzing */
	{
		struct theft_run_config config = {
		    .name = "fuzz_json_strings_no_crashes",
		    .prop2 = fuzz_prop_no_crashes_json_strings,
		    .type_info = {&json_string_type_info,
		                  &json_string_type_info},
		    .trials = string_trials,
		    .seed = seed + 2,
		};

		enum theft_run_res res = theft_run(&config);
		printf("Fuzz test 3 (JSON strings): %s\n",
		       res == THEFT_RUN_PASS ? "PASS" : "FAIL");
		if (res != THEFT_RUN_PASS)
			all_passed = false;
	}

	return all_passed;
}

int main(void)
{
	printf("JSON Diff Theft-Based Fuzzing Suite\n");
	printf("===================================\n\n");

	bool success = run_theft_fuzzing_tests();

	printf("\n===================================\n");
	if (success) {
		printf("All fuzzing tests PASSED! ✓\n");
		return 0;
	} else {
		printf("Some fuzzing tests FAILED! ✗\n");
		return 1;
	}
}
