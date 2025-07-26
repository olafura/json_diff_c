#include "parse_jsmn.h"
#include <stdlib.h>
#include <string.h>

// Recursive builder from JSMN tokens to cJSON, consuming tokens via *index
static cJSON *build_from_jsmn(const char *js, jsmntok_t *toks, int ntoks,
                              int *idx, const struct json_diff_options *opts) {
    if (*idx >= ntoks) return NULL;
    jsmntok_t *t = &toks[*idx];
    cJSON *item = NULL;
    int count = t->size;
    switch (t->type) {
    case JSMN_PRIMITIVE: {
        int len = t->end - t->start;
        char *val = (char *)malloc(len + 1);
        if (!val) return NULL;
        memcpy(val, js + t->start, len);
        val[len] = '\0';
        if (strcmp(val, "true") == 0 || strcmp(val, "false") == 0) {
            item = cJSON_CreateBool(val[0] == 't');
        } else if (strcmp(val, "null") == 0) {
            item = cJSON_CreateNull();
        } else {
            double d = strtod(val, NULL);
            item = cJSON_CreateNumber(d);
        }
        free(val);
        (*idx)++;
        break;
    }
    case JSMN_STRING: {
        int len = t->end - t->start;
        char *str = (char *)malloc(len + 1);
        if (!str) return NULL;
        memcpy(str, js + t->start, len);
        str[len] = '\0';
        item = cJSON_CreateString(str);
        free(str);
        (*idx)++;
        break;
    }
    case JSMN_OBJECT: {
        item = cJSON_CreateObject();
        (*idx)++;
        for (int i = 0; i < count; i++) {
            jsmntok_t *k = &toks[*idx];
            int len = k->end - k->start;
            char *key = (char *)malloc(len + 1);
            if (!key) { cJSON_Delete(item); return NULL; }
            memcpy(key, js + k->start, len);
            key[len] = '\0';
            (*idx)++;
            cJSON *val = build_from_jsmn(js, toks, ntoks, idx, opts);
            if (val)
                cJSON_AddItemToObject(item, key, val);
            free(key);
        }
        break;
    }
    case JSMN_ARRAY: {
        item = cJSON_CreateArray();
        (*idx)++;
        for (int i = 0; i < count; i++) {
            cJSON *val = build_from_jsmn(js, toks, ntoks, idx, opts);
            if (val)
                cJSON_AddItemToArray(item, val);
        }
        break;
    }
    default:
        (*idx)++;
        break;
    }
    return item;
}

cJSON *cjson_parse_jsmn(const char *text, const struct json_diff_options *opts) {
    size_t len = strlen(text);
    int max_tokens = (int)(len / 4);
    jsmntok_t *toks = (jsmntok_t *)malloc(sizeof(jsmntok_t) * max_tokens);
    if (!toks) return NULL;
    jsmn_parser p;
    jsmn_init(&p);
    int tokcount = jsmn_parse(&p, text, len, toks, max_tokens);
    if (tokcount < 0) {
        free(toks);
        return NULL;
    }
    int idx = 0;
    cJSON *root = build_from_jsmn(text, toks, tokcount, &idx, opts);
    free(toks);
    return root;
}

/**
 * json_diff_str - Parse two JSON strings via JSMN and diff them
 */
cJSON *json_diff_str(const char *left, const char *right,
                     const struct json_diff_options *opts)
{
    struct json_diff_arena arena;
    json_diff_arena_init(&arena, 1 << 20);
    struct json_diff_options local_opts = {
        .strict_equality = opts ? opts->strict_equality : true,
        .arena = &arena
    };
    cJSON *l = cjson_parse_jsmn(left, &local_opts);
    cJSON *r = cjson_parse_jsmn(right, &local_opts);
    cJSON *diff = NULL;
    if (l && r) diff = json_diff(l, r, &local_opts);
    cJSON_Delete(l);
    cJSON_Delete(r);
    json_diff_arena_cleanup(&arena);
    return diff;
}