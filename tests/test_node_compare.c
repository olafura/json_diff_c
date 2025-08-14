// SPDX-License-Identifier: Apache-2.0
/**
 * test_node_compare.c - Test JSON diff library against Node.js jsondiffpatch
 *
 * This test can optionally call the Node.js jsondiffpatch library for direct
 * comparison. Set environment variable JSON_DIFF_COMPARE_JS=1 to enable.
 *
 * Requires: npm install jsondiffpatch in the project root or globally
 */

#define __STDC_WANT_LIB_EXT1__ 1
#include "src/json_diff.h"
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * Call the Node.js jsondiffpatch helper to get the reference diff
 * Returns true on success, false if Node.js comparison unavailable
 */
static bool get_js_diff(const char *json_a, const char *json_b, char **result)
{
	/* Only run when explicitly enabled to avoid CI env issues */
	if (getenv("JSON_DIFF_COMPARE_JS") == NULL) {
		return false;
	}

	/* Check if helper exists */
	if (access("../jsondiffgo/js/test_helper.js", R_OK) != 0) {
		return false;
	}

	/* Create pipe for communication */
	int pipefd[2];
	if (pipe(pipefd) == -1) {
		perror("pipe");
		return false;
	}

	pid_t pid = fork();
	if (pid == -1) {
		perror("fork");
		close(pipefd[0]);
		close(pipefd[1]);
		return false;
	}

	if (pid == 0) {
		/* Child process */
		close(pipefd[0]); /* Close read end */

		/* Redirect stdout to pipe */
		if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
			perror("dup2");
			exit(1);
		}
		close(pipefd[1]);

		/* Execute node with the helper */
		execl("/usr/bin/node", "node",
		      "../jsondiffgo/js/test_helper.js", json_a, json_b, NULL);

		/* If execl fails, try alternative paths */
		execl("/usr/local/bin/node", "node",
		      "../jsondiffgo/js/test_helper.js", json_a, json_b, NULL);
		execl("/opt/homebrew/bin/node", "node",
		      "../jsondiffgo/js/test_helper.js", json_a, json_b, NULL);

		perror("execl");
		exit(1);
	} else {
		/* Parent process */
		close(pipefd[1]); /* Close write end */

		/* Read output */
		char buffer[4096];
		ssize_t bytes_read =
		    read(pipefd[0], buffer, sizeof(buffer) - 1);
		close(pipefd[0]);

		int status;
		waitpid(pid, &status, 0);

		if (WEXITSTATUS(status) != 0 || bytes_read <= 0) {
			return false;
		}

		buffer[bytes_read] = '\0';

		/* Remove trailing newline if present */
		if (bytes_read > 0 && buffer[bytes_read - 1] == '\n') {
			buffer[bytes_read - 1] = '\0';
		}

		*result = malloc(strlen(buffer) + 1);
		if (!*result) {
			return false;
		}
		strcpy(*result, buffer);
		return true;
	}
}

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
 * Test cases for Node.js comparison
 */
struct node_test_case {
	const char *name;
	const char *json_a;
	const char *json_b;
};

static const struct node_test_case node_test_cases[] = {
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
     .json_b = "{\"1\":[{\"1\":2}]}"},
    {.name = "identical objects",
     .json_a = "{\"test\":123,\"arr\":[1,2,3]}",
     .json_b = "{\"test\":123,\"arr\":[1,2,3]}"},
    {.name = "null values", .json_a = "null", .json_b = "null"},
    {.name = "empty objects", .json_a = "{}", .json_b = "{}"},
    {.name = "empty arrays", .json_a = "[]", .json_b = "[]"},
    {.name = "boolean values",
     .json_a = "{\"flag\":true}",
     .json_b = "{\"flag\":false}"},
    {.name = "string values",
     .json_a = "{\"msg\":\"hello\"}",
     .json_b = "{\"msg\":\"world\"}"}};

/**
 * Test against Node.js jsondiffpatch
 */
static void test_against_nodejs(void)
{
	printf("Testing against Node.js jsondiffpatch...\n");

	if (getenv("JSON_DIFF_COMPARE_JS") == NULL) {
		printf("  Skipped: Set JSON_DIFF_COMPARE_JS=1 to enable "
		       "Node.js comparison\n");
		return;
	}

	bool any_tested = false;
	int passed = 0;
	int total = 0;

	for (size_t i = 0;
	     i < sizeof(node_test_cases) / sizeof(node_test_cases[0]); i++) {
		const struct node_test_case *test_case = &node_test_cases[i];

		printf("  Testing: %s\n", test_case->name);
		total++;

		/* Get Node.js diff */
		char *js_diff_str = NULL;
		if (!get_js_diff(test_case->json_a, test_case->json_b,
		                 &js_diff_str)) {
			printf("    Skipped: Node.js comparison unavailable\n");
			continue;
		}

		any_tested = true;

		/* Get our diff */
		cJSON *our_diff =
		    json_diff_str(test_case->json_a, test_case->json_b, NULL);

		/* Parse JS diff (empty string or {} means no diff) */
		cJSON *js_diff = NULL;
		if (strlen(js_diff_str) > 0 && strcmp(js_diff_str, "{}") != 0) {
			js_diff = cJSON_Parse(js_diff_str);
			if (!js_diff) {
				printf(
				    "    ERROR: Failed to parse JS diff: %s\n",
				    js_diff_str);
				free(js_diff_str);
				if (our_diff)
					cJSON_Delete(our_diff);
				continue;
			}
		}

		/* Compare results */
		bool match = false;
		if (!our_diff && !js_diff) {
			match = true; /* Both report no diff */
		} else if (our_diff && js_diff) {
			match = cjson_equal(our_diff, js_diff);
		}

		if (match) {
			printf("    ✓ PASS\n");
			passed++;
		} else {
			printf("    ✗ FAIL\n");
			printf("      Our diff:  %s\n",
			       our_diff ? "present" : "NULL");
			printf("      JS diff:   %s\n",
			       js_diff ? "present" : "NULL");
			if (our_diff) {
				char *our_str = cJSON_Print(our_diff);
				printf("      Our:       %s\n",
				       our_str ? our_str : "NULL");
				free(our_str);
			}
			printf("      JS:        %s\n", js_diff_str);
		}

		free(js_diff_str);
		if (our_diff)
			cJSON_Delete(our_diff);
		if (js_diff)
			cJSON_Delete(js_diff);
	}

	if (any_tested) {
		printf("  Results: %d/%d tests passed\n", passed, total);
		if (passed != total) {
			printf("  WARNING: Some Node.js comparison tests "
			       "failed\n");
		}
	} else {
		printf("  No tests run - Node.js comparison unavailable\n");
		printf("  To enable: JSON_DIFF_COMPARE_JS=1 %s\n",
		       "and ensure node is in PATH");
	}
}

/**
 * Main test function
 */
int main(void)
{
	printf("=== JSON Diff Node.js Comparison Tests ===\n\n");

	test_against_nodejs();

	printf("\n=== Node.js comparison tests complete ===\n");
	return 0;
}
