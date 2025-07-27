// SPDX-License-Identifier: Apache-2.0
#define __STDC_WANT_LIB_EXT1__ 1
#define _GNU_SOURCE
#include "src/json_diff.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static double get_time_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static char *read_file(const char *path)
{
	FILE *f = fopen(path, "r");
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

int main(void)
{
	const char *paths[] = {"profile-data/cdc.json",
	                       "profile-data/edg.json"};
	char *bufs[2] = {0};
	for (int i = 0; i < 2; i++) {
		bufs[i] = read_file(paths[i]);
		if (!bufs[i]) {
			fputs("Failed to read '", stderr);
			fputs(paths[i], stderr);
			fputs("'\n", stderr);
			for (int j = 0; j < i; j++) {
				free(bufs[j]);
			}
			return 1;
		}
	}
	int iterations = 50;
	// Warm-up
	for (int i = 0; i < 5; i++) {
		cJSON *a = cJSON_Parse(bufs[0]);
		cJSON *b = cJSON_Parse(bufs[1]);
		cJSON_Delete(a);
		cJSON_Delete(b);
	}
	double t0 = get_time_ms();
	for (int i = 0; i < iterations; i++) {
		cJSON *a = cJSON_Parse(bufs[0]);
		cJSON *b = cJSON_Parse(bufs[1]);
		cJSON_Delete(a);
		cJSON_Delete(b);
	}
	double t1 = get_time_ms();
	printf("Parse benchmark: total = %.3f ms, avg = %.3f ms/iter\n",
	       t1 - t0, (t1 - t0) / iterations);

	free(bufs[0]);
	free(bufs[1]);
	return 0;
}
