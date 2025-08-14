/* SPDX-License-Identifier: Apache-2.0 */
#ifndef JSON_MYERS_H
#define JSON_MYERS_H

#include <cjson/cJSON.h>
#include "json_diff.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Compute jsondiffpatch-style array diff using Myers SES */
cJSON *json_myers_array_diff(const cJSON *left, const cJSON *right,
                             const struct json_diff_options *opts);

#ifdef __cplusplus
}
#endif

#endif /* JSON_MYERS_H */

