/* SPDX-License-Identifier: Apache-2.0 */
#ifndef JSMN_TREE_H
#define JSMN_TREE_H

#include <stddef.h>
#include <stdbool.h>
#include "jsmn.h"

/**
 * jsmntree_t - Parsed JSMN token tree view of a JSON string
 * @tokens: array of tokens
 * @count: number of tokens
 * @js: pointer to original JSON text
 */
typedef struct {
    jsmntok_t *tokens;
    size_t count;
    const char *js;
} jsmntree_t;

/**
 * jsmntree_init - Tokenize JSON text into a JSMN tree
 * @tree: pointer to tree to initialize
 * @js: NUL-terminated JSON text
 *
 * Return: 0 on success, negative on parse error
 */
int jsmntree_init(jsmntree_t *tree, const char *js);

/**
 * jsmntree_free - Free resources held by a JSMN tree
 * @tree: initialized tree
 */
void jsmntree_free(jsmntree_t *tree);

/**
 * jsmntree_num_children - Number of direct children of an array/object token
 * @tree: parsed tree
 * @idx: index of parent token
 *
 * Return: number of children, or 0 if not array/object
 */
size_t jsmntree_num_children(const jsmntree_t *tree, size_t idx);

/**
 * jsmntree_child - Get index of n'th direct child
 * @tree: parsed tree
 * @parent: index of parent token
 * @n: zero-based child index
 *
 * Return: token index of child, or -1 on error
 */
int jsmntree_child(const jsmntree_t *tree, int parent, size_t n);

/**
 * jsmntree_token_equal - Compare two tokens (and subtrees) for equality
 * @t1: first tree
 * @i1: token index in first tree
 * @t2: second tree
 * @i2: token index in second tree
 * @strict: strict number comparison
 *
 * Return: true if equal, false otherwise
 */
bool jsmntree_token_equal(const jsmntree_t *t1, int i1,
                          const jsmntree_t *t2, int i2,
                          bool strict);

#endif /* JSMN_TREE_H */