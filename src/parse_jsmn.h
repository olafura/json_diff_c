/* SPDX-License-Identifier: Apache-2.0 */
#ifndef PARSE_JSMN_H
#define PARSE_JSMN_H

#include <stddef.h>
#include "json_diff.h"
#include "jsmn.h"

/**
 * cjson_parse_jsmn - Parse JSON text via JSMN and build a cJSON tree
 * @text: NUL-terminated JSON input string
 * @opts: diff options (for arena, may be NULL)
 *
 * Return: newly created cJSON root (heap or arena) or NULL on error.
 */
cJSON *cjson_parse_jsmn(const char *text, const struct json_diff_options *opts);

#endif /* PARSE_JSMN_H */