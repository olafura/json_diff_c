// SPDX-License-Identifier: Apache-2.0
#include "src/json_diff.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

/**
 * read_file - Read entire file into a string
 * @filename: path to file
 *
 * Return: allocated string with file contents or NULL on failure
 */
static char *read_file(const char *filename)
{
	FILE *file = fopen(filename, "r");
	char *content = NULL;
	long length;

	if (!file) {
		printf("Could not open file: %s\n", filename);
		return NULL;
	}

	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return NULL;
	}
	length = ftell(file);
	if (length < 0) {
		fclose(file);
		return NULL;
	}
	if (fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return NULL;
	}

	content = malloc((size_t)length + 1);
	if (!content) {
		fclose(file);
		return NULL;
	}

	size_t bytes_read = fread(content, 1, (size_t)length, file);
	content[bytes_read] = '\0';
	fclose(file);

	return content;
}

/**
 * get_time_ms - Get current time in milliseconds
 *
 * Return: current time in milliseconds
 */
static double get_time_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

/**
 * profile_medium - Profile medium-sized JSON diff (equivalent to
 * ProfileMedium.scala)
 */
static void profile_medium(void)
{
	char *cdc_content = read_file("../profile-data/cdc.json");
	char *edg_content = read_file("../profile-data/edg.json");
	cJSON *cdc_json = NULL, *edg_json = NULL;
	double start_time, end_time;
	int i;

	if (!cdc_content || !edg_content) {
		printf("Could not read profile data files for medium test\n");
		printf("Run: cd .. && chmod +x profile-data/get_medium.sh && "
		       "./profile-data/get_medium.sh\n");
		goto cleanup;
	}

	cdc_json = cJSON_Parse(cdc_content);
	edg_json = cJSON_Parse(edg_content);

	if (!cdc_json || !edg_json) {
		printf("Could not parse JSON for medium test\n");
		goto cleanup;
	}

	printf("Running medium profile test (50 iterations)...\n");
	start_time = get_time_ms();

	for (i = 0; i < 50; i++) {
		cJSON *diff = json_diff(cdc_json, edg_json, NULL);
		if (diff) {
			cJSON_Delete(diff);
		}
	}

	end_time = get_time_ms();
	printf("Medium profile test completed in %.2f ms (avg: %.2f ms per "
	       "diff)\n",
	       end_time - start_time, (end_time - start_time) / 50.0);

cleanup:
	if (cdc_json)
		cJSON_Delete(cdc_json);
	if (edg_json)
		cJSON_Delete(edg_json);
	free(cdc_content);
	free(edg_content);
}

/**
 * profile_big - Profile large JSON diff (equivalent to ProfileBig.scala)
 */
static void profile_big(void)
{
	char *big1_content = read_file("../profile-data/ModernAtomic.json");
	char *big2_content = read_file("../profile-data/LegacyAtomic.json");
	cJSON *big1_json = NULL, *big2_json = NULL;
	double start_time, end_time;

	if (!big1_content || !big2_content) {
		printf("Could not read profile data files for big test\n");
		printf("Run: cd .. && chmod +x profile-data/get_big.sh && "
		       "./profile-data/get_big.sh\n");
		goto cleanup;
	}

	big1_json = cJSON_Parse(big1_content);
	big2_json = cJSON_Parse(big2_content);

	if (!big1_json || !big2_json) {
		printf("Could not parse JSON for big test\n");
		goto cleanup;
	}

	printf("Running big profile test (1 iteration)...\n");
	start_time = get_time_ms();

	cJSON *diff = json_diff(big1_json, big2_json, NULL);

	end_time = get_time_ms();
	printf("Big profile test completed in %.2f ms\n",
	       end_time - start_time);

	if (diff) {
		cJSON_Delete(diff);
	}

cleanup:
	if (big1_json)
		cJSON_Delete(big1_json);
	if (big2_json)
		cJSON_Delete(big2_json);
	free(big1_content);
	free(big2_content);
}

/**
 * profile_patch_performance - Test patch performance
 */
static void profile_patch_performance(void)
{
	cJSON *obj1, *obj2, *diff, *patched;
	cJSON *arr1, *arr2;
	double start_time, end_time;
	int i;

	printf("Running patch performance test...\n");

	/* Create large arrays for testing */
	obj1 = cJSON_CreateObject();
	arr1 = cJSON_CreateArray();
	for (i = 0; i < 1000; i++) {
		cJSON_AddItemToArray(arr1, cJSON_CreateNumber(i));
	}
	cJSON_AddItemToObject(obj1, "data", arr1);

	obj2 = cJSON_CreateObject();
	arr2 = cJSON_CreateArray();
	for (i = 0; i < 1000; i++) {
		cJSON_AddItemToArray(arr2, cJSON_CreateNumber(i * 2));
	}
	cJSON_AddItemToObject(obj2, "data", arr2);

	/* Test diff performance */
	start_time = get_time_ms();
	diff = json_diff(obj1, obj2, NULL);
	end_time = get_time_ms();
	printf("Large array diff completed in %.2f ms\n",
	       end_time - start_time);

	if (diff) {
		/* Test patch performance */
		start_time = get_time_ms();
		patched = json_patch(obj1, diff);
		end_time = get_time_ms();
		printf("Large array patch completed in %.2f ms\n",
		       end_time - start_time);

		if (patched) {
			/* Verify patch correctness */
			bool equal = json_value_equal(patched, obj2, false);
			printf("Patch correctness: %s\n",
			       equal ? "PASS" : "FAIL");
			cJSON_Delete(patched);
		}
		cJSON_Delete(diff);
	}

	cJSON_Delete(obj1);
	cJSON_Delete(obj2);
}

/**
 * main - Run all profile tests
 */
int main(void)
{
	printf("JSON Diff C Library - Performance Profile Tests\n");
	printf("===============================================\n\n");

	profile_medium();
	printf("\n");

	profile_big();
	printf("\n");

	profile_patch_performance();
	printf("\n");

	printf("Profile tests completed.\n");
	return 0;
}
