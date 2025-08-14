// SPDX-License-Identifier: Apache-2.0
#define __STDC_WANT_LIB_EXT1__ 1
#include "src/myers.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void assert_no_diff(const char *a, const char *b)
{
	cJSON *ja = cJSON_Parse(a);
	cJSON *jb = cJSON_Parse(b);
	assert(ja && jb);
	struct json_diff_options opts = {.strict_equality = true,
	                                 .arena = NULL};
	cJSON *d = json_myers_array_diff(ja, jb, &opts);
	if (d) {
		char *s = cJSON_Print(d);
		fprintf(stderr, "Expected no diff, got: %s\n", s ? s : "NULL");
		free(s);
	}
	assert(d == NULL);
	cJSON_Delete(ja);
	cJSON_Delete(jb);
}

static void assert_diff_eq(const char *a, const char *b, const char *expected)
{
	cJSON *ja = cJSON_Parse(a);
	cJSON *jb = cJSON_Parse(b);
	cJSON *je = cJSON_Parse(expected);
	assert(ja && jb && je);
	struct json_diff_options opts = {.strict_equality = true,
	                                 .arena = NULL};
	cJSON *d = json_myers_array_diff(ja, jb, &opts);
	if (!json_value_equal(d, je, false)) {
		char *got = cJSON_Print(d);
		char *exp = cJSON_Print(je);
		fprintf(stderr, "Diff mismatch\nGot: %s\nExp: %s\n",
		        got ? got : "NULL", exp ? exp : "NULL");
		free(got);
		free(exp);
	}
	assert(json_value_equal(d, je, false));
	cJSON_Delete(d);
	cJSON_Delete(ja);
	cJSON_Delete(jb);
	cJSON_Delete(je);
}

int main(void)
{
	// Empty sequences
	assert_no_diff("[]", "[]");

	// All equal
	assert_no_diff("[1,2,3]", "[1,2,3]");

	// Inserts only
	assert_diff_eq(
	    "[]", "[\"a\",\"b\",\"c\"]",
	    "{\"0\":[\"a\"],\"1\":[\"b\"],\"2\":[\"c\"],\"_t\":\"a\"}");

	// Deletes only
	assert_diff_eq(
	    "[9,8,7]", "[]",
	    "{\"_0\":[9,0,0],\"_1\":[8,0,0],\"_2\":[7,0,0],\"_t\":\"a\"}");

	// Grouping: middle insert
	assert_diff_eq("[\"a\",\"b\",\"c\"]", "[\"a\",\"b\",\"d\",\"c\"]",
	               "{\"2\":[\"d\"],\"_t\":\"a\"}");

	// Paper middle insert
	assert_diff_eq("[1,2,3]", "[1,4,2,3]", "{\"1\":[4],\"_t\":\"a\"}");

	// Paper middle delete
	assert_diff_eq("[1,4,2,3]", "[1,2,3]", "{\"_1\":[4,0,0],\"_t\":\"a\"}");

	// Nested insert from scalar
	assert_diff_eq("[1]", "[[1]]",
	               "{\"0\":[[1]],\"_0\":[1,0,0],\"_t\":\"a\"}");

	// Nested delete to scalar
	assert_diff_eq("[[1]]", "[1]",
	               "{\"0\":[1],\"_0\":[[1],0,0],\"_t\":\"a\"}");

	// Complex: [1, {"1":1}] -> [{"1":2}] must include trailing deletion
	assert_diff_eq("[1,{\"1\":1}]", "[{\"1\":2}]",
	               "{\"0\":[{\"1\":2}],\"_0\":[1,0,0],\"_1\":[{\"1\":1},0,"
	               "0],\"_t\":\"a\"}");

	printf("Myers array diff tests passed\n");
	return 0;
}
