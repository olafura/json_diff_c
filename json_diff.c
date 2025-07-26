// SPDX-License-Identifier: Apache-2.0
#define _GNU_SOURCE
#include <math.h>
#include "json_diff.h"

#define ARRAY_MARKER "_t"
#define ARRAY_MARKER_VALUE "a"

/**
 * json_value_equal - Compare two cJSON values for equality
 * @left: first value
 * @right: second value
 * @strict: use strict equality for numbers
 *
 * Return: true if equal, false otherwise
 */
bool json_value_equal(const cJSON *left, const cJSON *right, bool strict)
{
	int i;

	if (!left && !right)
		return true;
	if (!left || !right)
		return false;

	if (left->type != right->type)
		return false;

	switch (left->type) {
	case cJSON_NULL:
		return true;
	case cJSON_False:
	case cJSON_True:
		return left->type == right->type;
	case cJSON_Number:
		if (strict)
			return left->valuedouble == right->valuedouble;
		else
			return fabs(left->valuedouble - right->valuedouble) < 1e-9;
	case cJSON_String:
		return strcmp(left->valuestring, right->valuestring) == 0;
	case cJSON_Array:
		if (cJSON_GetArraySize(left) != cJSON_GetArraySize(right))
			return false;
		for (i = 0; i < cJSON_GetArraySize(left); i++) {
			if (!json_value_equal(cJSON_GetArrayItem(left, i),
					      cJSON_GetArrayItem(right, i),
					      strict))
				return false;
		}
		return true;
	case cJSON_Object:
		if (cJSON_GetArraySize(left) != cJSON_GetArraySize(right))
			return false;
		
		cJSON *left_item = left->child;
		while (left_item) {
			cJSON *right_item = cJSON_GetObjectItem(right, left_item->string);
			if (!right_item ||
			    !json_value_equal(left_item, right_item, strict))
				return false;
			left_item = left_item->next;
		}
		return true;
	}

	return false;
}

/**
 * create_change_array - Create a change array [old_value, new_value]
 * @old_val: old value
 * @new_val: new value
 *
 * Return: cJSON array or NULL on failure
 */
cJSON *create_change_array(const cJSON *old_val, const cJSON *new_val)
{
	cJSON *array = cJSON_CreateArray();
	if (!array)
		return NULL;

	cJSON *old_copy = cJSON_Duplicate(old_val, 1);
	cJSON *new_copy = cJSON_Duplicate(new_val, 1);
	
	if (!old_copy || !new_copy) {
		cJSON_Delete(array);
		cJSON_Delete(old_copy);
		cJSON_Delete(new_copy);
		return NULL;
	}

	cJSON_AddItemToArray(array, old_copy);
	cJSON_AddItemToArray(array, new_copy);
	return array;
}

/**
 * create_addition_array - Create an addition array [new_value]
 * @new_val: new value
 *
 * Return: cJSON array or NULL on failure
 */
cJSON *create_addition_array(const cJSON *new_val)
{
	cJSON *array = cJSON_CreateArray();
	if (!array)
		return NULL;

	cJSON *new_copy = cJSON_Duplicate(new_val, 1);
	if (!new_copy) {
		cJSON_Delete(array);
		return NULL;
	}

	cJSON_AddItemToArray(array, new_copy);
	return array;
}

/**
 * create_deletion_array - Create a deletion array [old_value, 0, 0]
 * @old_val: old value
 *
 * Return: cJSON array or NULL on failure
 */
cJSON *create_deletion_array(const cJSON *old_val)
{
	cJSON *array = cJSON_CreateArray();
	if (!array)
		return NULL;

	cJSON *old_copy = cJSON_Duplicate(old_val, 1);
	cJSON *zero1 = cJSON_CreateNumber(0);
	cJSON *zero2 = cJSON_CreateNumber(0);
	
	if (!old_copy || !zero1 || !zero2) {
		cJSON_Delete(array);
		cJSON_Delete(old_copy);
		cJSON_Delete(zero1);
		cJSON_Delete(zero2);
		return NULL;
	}

	cJSON_AddItemToArray(array, old_copy);
	cJSON_AddItemToArray(array, zero1);
	cJSON_AddItemToArray(array, zero2);
	return array;
}

/**
 * diff_arrays - Create diff for two cJSON arrays
 * @left: first array
 * @right: second array
 * @opts: diff options
 *
 * Return: diff object or NULL if arrays are equal
 */
static cJSON *diff_arrays(const cJSON *left, const cJSON *right,
			  const struct json_diff_options *opts)
{
	cJSON *diff_obj = cJSON_CreateObject();
	char index_str[32];
	int left_size = cJSON_GetArraySize(left);
	int right_size = cJSON_GetArraySize(right);
	int max_size = (left_size > right_size) ? left_size : right_size;
	bool has_changes = false;
	int i;

	if (!diff_obj)
		return NULL;

	/* Simple implementation: mark all changes */
	for (i = 0; i < max_size; i++) {
		cJSON *left_item = (i < left_size) ? cJSON_GetArrayItem(left, i) : NULL;
		cJSON *right_item = (i < right_size) ? cJSON_GetArrayItem(right, i) : NULL;

		if (left_item && right_item) {
			if (!json_value_equal(left_item, right_item, opts->strict_equality)) {
				cJSON *sub_diff = json_diff(left_item, right_item, opts);
				if (sub_diff) {
					snprintf(index_str, sizeof(index_str), "%d", i);
					cJSON_AddItemToObject(diff_obj, index_str, sub_diff);
					has_changes = true;
				}
			}
		} else if (left_item) {
			/* Deletion */
			cJSON *del_array = create_deletion_array(left_item);
			if (del_array) {
				snprintf(index_str, sizeof(index_str), "_%d", i);
				cJSON_AddItemToObject(diff_obj, index_str, del_array);
				has_changes = true;
			}
		} else if (right_item) {
			/* Insertion */
			cJSON *ins_array = create_addition_array(right_item);
			if (ins_array) {
				snprintf(index_str, sizeof(index_str), "%d", i);
				cJSON_AddItemToObject(diff_obj, index_str, ins_array);
				has_changes = true;
			}
		}
	}

	if (has_changes) {
		cJSON_AddStringToObject(diff_obj, ARRAY_MARKER, ARRAY_MARKER_VALUE);
		return diff_obj;
	}

	cJSON_Delete(diff_obj);
	return NULL;
}

/**
 * json_diff - Create a diff between two cJSON values
 * @left: first JSON value
 * @right: second JSON value
 * @opts: diff options (can be NULL for defaults)
 *
 * Return: diff object or NULL if values are equal
 */
cJSON *json_diff(const cJSON *left, const cJSON *right,
		 const struct json_diff_options *opts)
{
	struct json_diff_options default_opts = { .strict_equality = true };
	cJSON *diff_obj;
	bool has_changes = false;

	if (!opts)
		opts = &default_opts;

	if (json_value_equal(left, right, opts->strict_equality))
		return NULL;

	if (!left || !right || left->type != right->type || 
	    (left->type != cJSON_Object && left->type != cJSON_Array)) {
		/* Simple value change */
		return create_change_array(left, right);
	}

	if (left->type == cJSON_Array)
		return diff_arrays(left, right, opts);

	/* Object diff */
	diff_obj = cJSON_CreateObject();
	if (!diff_obj)
		return NULL;

	/* Check all keys in left object */
	cJSON *left_item = left->child;
	while (left_item) {
		const char *key = left_item->string;
		cJSON *right_item = cJSON_GetObjectItem(right, key);

		if (!right_item) {
			/* Key deleted */
			cJSON *del_array = create_deletion_array(left_item);
			cJSON_AddItemToObject(diff_obj, key, del_array);
			has_changes = true;
		} else {
			/* Key exists in both, check for changes */
			cJSON *sub_diff = json_diff(left_item, right_item, opts);
			if (sub_diff) {
				cJSON_AddItemToObject(diff_obj, key, sub_diff);
				has_changes = true;
			}
		}
		left_item = left_item->next;
	}

	/* Check for new keys in right object */
	cJSON *right_item = right->child;
	while (right_item) {
		const char *key = right_item->string;
		cJSON *left_item = cJSON_GetObjectItem(left, key);

		if (!left_item) {
			/* Key added */
			cJSON *add_array = create_addition_array(right_item);
			cJSON_AddItemToObject(diff_obj, key, add_array);
			has_changes = true;
		}
		right_item = right_item->next;
	}

	if (has_changes)
		return diff_obj;

	cJSON_Delete(diff_obj);
	return NULL;
}

/**
 * patch_array - Apply array diff to an array
 * @original: original array
 * @diff: array diff object
 *
 * Return: patched array or NULL on failure
 */
static cJSON *patch_array(const cJSON *original, const cJSON *diff)
{
	cJSON *result = cJSON_CreateArray();
	cJSON *orig_item;
	int orig_size = cJSON_GetArraySize(original);
	int i;

	if (!result)
		return NULL;

	/* Copy original array */
	for (i = 0; i < orig_size; i++) {
		orig_item = cJSON_GetArrayItem(original, i);
		if (orig_item) {
			cJSON *copy = cJSON_Duplicate(orig_item, 1);
			if (copy)
				cJSON_AddItemToArray(result, copy);
		}
	}

	/* Apply changes from diff */
	cJSON *diff_item = diff->child;
	while (diff_item) {
		const char *key = diff_item->string;

		if (strcmp(key, ARRAY_MARKER) == 0) {
			diff_item = diff_item->next;
			continue; /* Skip array marker */
		}

		if (key[0] == '_') {
			/* Deletion - key format is "_index" */
			int index = atoi(key + 1);
			if (index >= 0 && index < cJSON_GetArraySize(result)) {
				cJSON_DeleteItemFromArray(result, index);
			}
		} else {
			/* Addition or modification - key is index */
			int index = atoi(key);
			if (cJSON_IsArray(diff_item)) {
				int array_size = cJSON_GetArraySize(diff_item);
				if (array_size == 1) {
					/* Addition */
					cJSON *new_val = cJSON_Duplicate(cJSON_GetArrayItem(diff_item, 0), 1);
					if (new_val) {
						if (index >= cJSON_GetArraySize(result)) {
							cJSON_AddItemToArray(result, new_val);
						} else {
							cJSON_InsertItemInArray(result, index, new_val);
						}
					}
				} else if (array_size == 2) {
					/* Replacement */
					cJSON *new_val = cJSON_Duplicate(cJSON_GetArrayItem(diff_item, 1), 1);
					if (new_val && index < cJSON_GetArraySize(result)) {
						cJSON_ReplaceItemInArray(result, index, new_val);
					}
				}
			} else {
				/* Nested diff */
				if (index < cJSON_GetArraySize(result)) {
					cJSON *orig_val = cJSON_GetArrayItem(result, index);
					if (orig_val) {
						cJSON *patched = json_patch(orig_val, diff_item);
						if (patched) {
							cJSON_ReplaceItemInArray(result, index, patched);
						}
					}
				}
			}
		}
		diff_item = diff_item->next;
	}

	return result;
}

/**
 * json_patch - Apply a diff to a cJSON value
 * @original: original JSON value
 * @diff: diff to apply
 *
 * Return: patched JSON value or NULL on failure
 */
cJSON *json_patch(const cJSON *original, const cJSON *diff)
{
	cJSON *result;

	if (!original || !diff)
		return NULL;

	if (cJSON_IsArray(diff) && cJSON_GetArraySize(diff) == 2) {
		/* Simple value replacement */
		return cJSON_Duplicate(cJSON_GetArrayItem(diff, 1), 1);
	}

	if (!cJSON_IsObject(diff))
		return cJSON_Duplicate(original, 1);

	/* Check if this is an array diff */
	if (cJSON_GetObjectItem(diff, ARRAY_MARKER)) {
		if (cJSON_IsArray(original)) {
			return patch_array(original, diff);
		} else {
			return cJSON_Duplicate(original, 1);
		}
	}

	if (!cJSON_IsObject(original))
		return cJSON_Duplicate(original, 1);

	result = cJSON_Duplicate(original, 1);
	if (!result)
		return NULL;

	/* Apply all changes from diff */
	cJSON *diff_item = diff->child;
	while (diff_item) {
		const char *key = diff_item->string;

		if (cJSON_IsArray(diff_item)) {
			int array_size = cJSON_GetArraySize(diff_item);
			if (array_size == 1) {
				/* Addition */
				cJSON *new_val = cJSON_Duplicate(cJSON_GetArrayItem(diff_item, 0), 1);
				if (new_val) {
					cJSON_DeleteItemFromObject(result, key);
					cJSON_AddItemToObject(result, key, new_val);
				}
			} else if (array_size == 3) {
				/* Deletion - remove key */
				cJSON_DeleteItemFromObject(result, key);
			} else if (array_size == 2) {
				/* Replacement */
				cJSON *new_val = cJSON_Duplicate(cJSON_GetArrayItem(diff_item, 1), 1);
				if (new_val) {
					cJSON_DeleteItemFromObject(result, key);
					cJSON_AddItemToObject(result, key, new_val);
				}
			}
		} else {
			/* Nested diff */
			cJSON *orig_val = cJSON_GetObjectItem(result, key);
			if (orig_val) {
				cJSON *patched = json_patch(orig_val, diff_item);
				if (patched) {
					cJSON_DeleteItemFromObject(result, key);
					cJSON_AddItemToObject(result, key, patched);
				}
			}
		}
		diff_item = diff_item->next;
	}

	return result;
}
