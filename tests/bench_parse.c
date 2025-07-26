// SPDX-License-Identifier: Apache-2.0
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include "json_diff.h"

static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t _n = fread(buf, 1, len, f);
    (void)_n;
    buf[len] = '\0';
    fclose(f);
    return buf;
}

int main(void) {
    const char *paths[] = {"profile-data/cdc.json", "profile-data/edg.json"};
    char *bufs[2] = {0};
    for (int i = 0; i < 2; i++) {
        bufs[i] = read_file(paths[i]);
        if (!bufs[i]) {
            fprintf(stderr, "Failed to read '%s'\n", paths[i]);
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