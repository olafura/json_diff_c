#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "tests/jsmn.h"
#include <string.h>
#include <string.h>

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
    size_t r = fread(buf, 1, len, f);
    (void)r;
    buf[len] = '\0';
    fclose(f);
    return buf;
}

int main(void) {
    const char *left = "profile-data/cdc.json";
    const char *right = "profile-data/edg.json";
    char *jb[2];
    jsmntok_t tokens[100000];
    jsmn_parser p;

    jb[0] = read_file(left);
    jb[1] = read_file(right);
    if (!jb[0] || !jb[1]) return 1;

    // Warmup
    for (int i = 0; i < 5; i++) {
        jsmn_init(&p);
        jsmn_parse(&p, jb[0], strlen(jb[0]), tokens, 100000);
        jsmn_init(&p);
        jsmn_parse(&p, jb[1], strlen(jb[1]), tokens, 100000);
    }

    int iterations = 50;
    double t0 = get_time_ms();
    for (int i = 0; i < iterations; i++) {
        jsmn_init(&p);
        jsmn_parse(&p, jb[0], strlen(jb[0]), tokens, 100000);
        jsmn_init(&p);
        jsmn_parse(&p, jb[1], strlen(jb[1]), tokens, 100000);
    }
    double t1 = get_time_ms();

    printf("jsmn parse benchmark: total = %.3f ms, avg = %.3f ms/iter\n",
           t1 - t0, (t1 - t0) / iterations);
    free(jb[0]); free(jb[1]);
    return 0;
}