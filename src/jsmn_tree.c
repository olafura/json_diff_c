#include "jsmn_tree.h"
#include <stdlib.h>
#include <string.h>

/**
 * Compute size of subtree (token + all nested children)
 */
static int subtree_size(const jsmntree_t *tree, int idx) {
    jsmntok_t *tok = &tree->tokens[idx];
    int cnt = 1;
    if (tok->type == JSMN_OBJECT || tok->type == JSMN_ARRAY) {
        int children = tok->size;
        int i = idx + 1;
        for (int c = 0; c < children; c++) {
            int sz = subtree_size(tree, i);
            cnt += sz;
            i += sz;
        }
    }
    return cnt;
}

int jsmntree_init(jsmntree_t *tree, const char *js) {
    size_t len = strlen(js);
    size_t max_tokens = len / 4 + 1;
    tree->tokens = malloc(max_tokens * sizeof(jsmntok_t));
    if (!tree->tokens) return -1;
    jsmn_parser p;
    jsmn_init(&p);
    int rc = jsmn_parse(&p, js, len, tree->tokens, (unsigned)max_tokens);
    if (rc < 0) {
        free(tree->tokens);
        tree->tokens = NULL;
        return rc;
    }
    tree->count = rc;
    tree->js = js;
    return 0;
}

void jsmntree_free(jsmntree_t *tree) {
    free(tree->tokens);
    tree->tokens = NULL;
    tree->count = 0;
    tree->js = NULL;
}

size_t jsmntree_num_children(const jsmntree_t *tree, size_t idx) {
    if (idx < tree->count) {
        jsmntok_t *tok = &tree->tokens[idx];
        if (tok->type == JSMN_OBJECT || tok->type == JSMN_ARRAY)
            return (size_t)tok->size;
    }
    return 0;
}

int jsmntree_child(const jsmntree_t *tree, int parent, size_t n) {
    if (parent < 0 || parent >= (int)tree->count) return -1;
    jsmntok_t *tok = &tree->tokens[parent];
    if (tok->type != JSMN_OBJECT && tok->type != JSMN_ARRAY) return -1;
    if (n >= (size_t)tok->size) return -1;
    int idx = parent + 1;
    for (size_t i = 0; i < n; i++) {
        int sz = subtree_size(tree, idx);
        idx += sz;
    }
    return idx;
}

bool jsmntree_token_equal(const jsmntree_t *t1, int i1,
                          const jsmntree_t *t2, int i2,
                          bool strict) {
    if (i1 < 0 || i2 < 0 || i1 >= (int)t1->count || i2 >= (int)t2->count)
        return false;
    jsmntok_t *a = &t1->tokens[i1];
    jsmntok_t *b = &t2->tokens[i2];
    if (a->type != b->type) return false;
    const char *s1 = t1->js + a->start;
    const char *s2 = t2->js + b->start;
    size_t len1 = a->end - a->start;
    size_t len2 = b->end - b->start;
    if (a->type == JSMN_STRING || a->type == JSMN_PRIMITIVE) {
        if (len1 != len2) return false;
        return (strncmp(s1, s2, len1) == 0);
    } else if (a->type == JSMN_OBJECT) {
        size_t cnt = a->size;
        for (size_t c = 0; c < cnt; c++) {
            int k1 = jsmntree_child(t1, i1, 2*c + 0);
            int v1 = jsmntree_child(t1, i1, 2*c + 1);
            int k2 = jsmntree_child(t2, i2, 2*c + 0);
            int v2 = jsmntree_child(t2, i2, 2*c + 1);
            if (!jsmntree_token_equal(t1, k1, t2, k2, strict) ||
                !jsmntree_token_equal(t1, v1, t2, v2, strict))
                return false;
        }
        return true;
    } else if (a->type == JSMN_ARRAY) {
        size_t cnt = a->size;
        for (size_t c = 0; c < cnt; c++) {
            int elem1 = jsmntree_child(t1, i1, c);
            int elem2 = jsmntree_child(t2, i2, c);
            if (!jsmntree_token_equal(t1, elem1, t2, elem2, strict))
                return false;
        }
        return true;
    }
    return false;
}