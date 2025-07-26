// SPDX-License-Identifier: Apache-2.0
#define _GNU_SOURCE // for asprintf if needed
#include "src/json_diff.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static char *read_file(const char *filename)
{
	FILE *f = fopen(filename, "r");
	if (!f)
		return NULL;
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return NULL;
	}
	long len = ftell(f);
	if (len < 0) {
		fclose(f);
		return NULL;
	}
	if (fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return NULL;
	}
	char *buf = malloc((size_t)len + 1);
	if (!buf) {
		fclose(f);
		return NULL;
	}
	size_t n = fread(buf, 1, (size_t)len, f);
	buf[n] = '\0';
	fclose(f);
	return buf;
}

static double get_time_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

int main(void)
{
	const char *paths[][2] = {
	    {"profile-data/cdc.json", "profile-data/edg.json"},
	    {"../profile-data/cdc.json", "../profile-data/edg.json"}};
	char *left_buf = NULL, *right_buf = NULL;
	for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
		left_buf = read_file(paths[i][0]);
		right_buf = read_file(paths[i][1]);
		if (left_buf && right_buf)
			break;
		free(left_buf);
		free(right_buf);
		left_buf = right_buf = NULL;
	}
	if (!left_buf || !right_buf) {
		fputs("Failed to load input files in profile-data/ or "
		      "../profile-data/\n",
		      stderr);
		return 1;
	}

	cJSON *left = cJSON_Parse(left_buf);
	cJSON *right = cJSON_Parse(right_buf);
	if (!left || !right) {
		fputs("Failed to parse JSON inputs\n", stderr);
		free(left_buf);
		free(right_buf);
		return 1;
	}

	// Set up arena-based allocations for diff
	struct json_diff_arena arena;
	json_diff_arena_init(&arena, 1 << 20); // 1MB initial arena
	struct json_diff_options opts = {.strict_equality = true,
	                                 .arena = &arena};

	// Warm-up iterations
	for (int i = 0; i < 5; i++) {
		cJSON *d = json_diff(left, right, &opts);
		(void)d;
	}

	const int iterations = 50;
	double t0 = get_time_ms();
	for (int i = 0; i < iterations; i++) {
		cJSON *d = json_diff(left, right, &opts);
		(void)d;
	}
	double t1 = get_time_ms();

	double total = t1 - t0;
	// Display total time in ms and average per iteration in microseconds
	printf("Medium diff benchmark: total = %.3f ms, avg = %.3f us/iter\n",
	       total, (total * 1000.0) / iterations);

	cJSON_Delete(left);
	cJSON_Delete(right);
	json_diff_arena_cleanup(&arena);
	free(left_buf);
	free(right_buf);
	return 0;
}
