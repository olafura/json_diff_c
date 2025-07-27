// SPDX-License-Identifier: Apache-2.0
#define __STDC_WANT_LIB_EXT1__ 1
#include "json_diff.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Declare C11 Annex K secure functions if not available */
#ifndef __STDC_LIB_EXT1__
typedef size_t rsize_t;
typedef int errno_t;

errno_t snprintf_s(char *restrict s, rsize_t n, const char *restrict format, ...);
errno_t vsnprintf_s(char *restrict s, rsize_t n, const char *restrict format, va_list args);
errno_t memcpy_s(void *restrict s1, rsize_t s1max, const void *restrict s2, rsize_t n);

/* Simple implementations for systems without Annex K */
#include <stdarg.h>
errno_t snprintf_s(char *restrict s, rsize_t n, const char *restrict format, ...)
{
    if (!s || n == 0) return EINVAL;
    va_list args;
    va_start(args, format);
    int result = vsnprintf_s(s, n, format, args);
    va_end(args);
    return (result >= 0 && (size_t)result < n) ? 0 : ERANGE;
}

errno_t vsnprintf_s(char *restrict s, rsize_t n, const char *restrict format, va_list args)
{
    if (!s || n == 0) return EINVAL;
    int result = vsnprintf(s, n, format, args);
    return (result >= 0 && (size_t)result < n) ? 0 : ERANGE;
}

errno_t memcpy_s(void *restrict s1, rsize_t s1max, const void *restrict s2, rsize_t n)
{
    if (!s1 || !s2 || s1max < n) return EINVAL;
    memcpy(s1, s2, n);
    return 0;
}
#endif

/* Include for arena allocator */
#include <stdlib.h>
#include <string.h>

/* Arena-based allocation hooks for diff trees */
static __thread struct json_diff_arena *current_arena = NULL;

static void *arena_malloc(size_t size)
{
	size_t off = (current_arena->offset + sizeof(void *) - 1) &
	             ~(sizeof(void *) - 1);
	if (off + size > current_arena->capacity) {
		size_t newcap = current_arena->capacity
		                    ? current_arena->capacity * 2
		                    : size * 2;
		while (newcap < off + size)
			newcap *= 2;
		char *newbuf = realloc(current_arena->buf, newcap);
		if (!newbuf)
			return NULL;
		current_arena->buf = newbuf;
		current_arena->capacity = newcap;
	}
	void *ptr = current_arena->buf + off;
	current_arena->offset = off + size;
	return ptr;
}

static void arena_free(void *ptr) { (void)ptr; }

void json_diff_arena_init(struct json_diff_arena *arena,
                          size_t initial_capacity)
{
	arena->buf = malloc(initial_capacity);
	arena->capacity = initial_capacity;
	arena->offset = 0;
}

void json_diff_arena_cleanup(struct json_diff_arena *arena)
{
	free(arena->buf);
	arena->buf = NULL;
	arena->capacity = arena->offset = 0;
}

#define ARRAY_MARKER "_t"
#define ARRAY_MARKER_VALUE "a"

/**
 * json_value_equal - Compare two cJSON values for equality (optimized)
 * @left: first value
 * @right: second value
 * @strict: use strict equality for numbers
 *
 * Return: true if equal, false otherwise
 */
bool json_value_equal(const cJSON *left, const cJSON *right, bool strict)
{
	/* Fast pointer equality check */
	if (left == right)
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
		return true; /* Same type already checked */
	case cJSON_Number:
		if (strict)
			return left->valuedouble == right->valuedouble;
		else
			return fabs(left->valuedouble - right->valuedouble) <
			       1e-9;
	case cJSON_String:
		/* Fast pointer and length check first */
		if (left->valuestring == right->valuestring)
			return true;
		if (!left->valuestring || !right->valuestring)
			return false;
		/* Quick length check before strcmp */
		size_t left_len = strlen(left->valuestring);
		size_t right_len = strlen(right->valuestring);
		if (left_len != right_len)
			return false;
		return memcmp(left->valuestring, right->valuestring,
		              left_len) == 0;
	case cJSON_Array: {
		int left_size = cJSON_GetArraySize(left);
		int right_size = cJSON_GetArraySize(right);
		if (left_size != right_size)
			return false;

		/* Use direct child iteration for better performance */
		cJSON *left_item = left->child;
		cJSON *right_item = right->child;
		while (left_item && right_item) {
			if (!json_value_equal(left_item, right_item, strict))
				return false;
			left_item = left_item->next;
			right_item = right_item->next;
		}
		return left_item == NULL && right_item == NULL;
	}
	case cJSON_Object: {
		int left_size = cJSON_GetArraySize(left);
		int right_size = cJSON_GetArraySize(right);
		if (left_size != right_size)
			return false;

		cJSON *left_item = left->child;
		while (left_item) {
			cJSON *right_item =
			    cJSON_GetObjectItem(right, left_item->string);
			if (!right_item ||
			    !json_value_equal(left_item, right_item, strict))
				return false;
			left_item = left_item->next;
		}
		return true;
	}
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

	/* Use references for objects/arrays/strings, copy scalars */
	cJSON *old_item = NULL;
	if (cJSON_IsObject(old_val)) {
		old_item = cJSON_CreateObjectReference(old_val);
	} else if (cJSON_IsArray(old_val)) {
		old_item = cJSON_CreateArrayReference(old_val);
	} else if (cJSON_IsString(old_val)) {
		old_item = cJSON_CreateString(old_val->valuestring);
	} else if (cJSON_IsNumber(old_val)) {
		old_item = cJSON_CreateNumber(old_val->valuedouble);
	} else if (cJSON_IsBool(old_val)) {
		old_item = cJSON_CreateBool(cJSON_IsTrue(old_val));
	} else {
		old_item = cJSON_CreateNull();
	}
	cJSON *new_item = NULL;
	if (cJSON_IsObject(new_val)) {
		new_item = cJSON_CreateObjectReference(new_val);
	} else if (cJSON_IsArray(new_val)) {
		new_item = cJSON_CreateArrayReference(new_val);
	} else if (cJSON_IsString(new_val)) {
		new_item = cJSON_CreateString(new_val->valuestring);
	} else if (cJSON_IsNumber(new_val)) {
		new_item = cJSON_CreateNumber(new_val->valuedouble);
	} else if (cJSON_IsBool(new_val)) {
		new_item = cJSON_CreateBool(cJSON_IsTrue(new_val));
	} else {
		new_item = cJSON_CreateNull();
	}
	if (!old_item || !new_item) {
		cJSON_Delete(array);
		cJSON_Delete(old_item);
		cJSON_Delete(new_item);
		return NULL;
	}
	cJSON_AddItemToArray(array, old_item);
	cJSON_AddItemToArray(array, new_item);
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

	/* Reference new value to avoid deep copy */
	cJSON *new_item = NULL;
	if (cJSON_IsObject(new_val)) {
		new_item = cJSON_CreateObjectReference(new_val);
	} else if (cJSON_IsArray(new_val)) {
		new_item = cJSON_CreateArrayReference(new_val);
	} else if (cJSON_IsString(new_val)) {
		new_item = cJSON_CreateString(new_val->valuestring);
	} else if (cJSON_IsNumber(new_val)) {
		new_item = cJSON_CreateNumber(new_val->valuedouble);
	} else if (cJSON_IsBool(new_val)) {
		new_item = cJSON_CreateBool(cJSON_IsTrue(new_val));
	} else {
		new_item = cJSON_CreateNull();
	}
	if (!new_item) {
		cJSON_Delete(array);
		return NULL;
	}
	cJSON_AddItemToArray(array, new_item);
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

	/* Reference old value and add zeros for deletion */
	cJSON *old_item = NULL;
	if (cJSON_IsObject(old_val)) {
		old_item = cJSON_CreateObjectReference(old_val);
	} else if (cJSON_IsArray(old_val)) {
		old_item = cJSON_CreateArrayReference(old_val);
	} else if (cJSON_IsString(old_val)) {
		old_item = cJSON_CreateString(old_val->valuestring);
	} else if (cJSON_IsNumber(old_val)) {
		old_item = cJSON_CreateNumber(old_val->valuedouble);
	} else if (cJSON_IsBool(old_val)) {
		old_item = cJSON_CreateBool(cJSON_IsTrue(old_val));
	} else {
		old_item = cJSON_CreateNull();
	}
	cJSON *zero1 = cJSON_CreateNumber(0);
	cJSON *zero2 = cJSON_CreateNumber(0);
	if (!old_item || !zero1 || !zero2) {
		cJSON_Delete(array);
		cJSON_Delete(old_item);
		cJSON_Delete(zero1);
		cJSON_Delete(zero2);
		return NULL;
	}
	cJSON_AddItemToArray(array, old_item);
	cJSON_AddItemToArray(array, zero1);
	cJSON_AddItemToArray(array, zero2);
	return array;
}

/**
 * myers_diff_arrays - Fast array diff using simple linear comparison
 * @left: first array
 * @right: second array
 * @opts: diff options
 *
 * Return: diff object or NULL if arrays are equal
 */
static cJSON *myers_diff_arrays(const cJSON *left, const cJSON *right,
                                const struct json_diff_options *opts)
{
	int left_size = cJSON_GetArraySize(left);
	int right_size = cJSON_GetArraySize(right);

	/* Quick equality check using direct child iteration */
	if (left_size == right_size) {
		cJSON *left_item = left->child;
		cJSON *right_item = right->child;
		bool all_equal = true;

		while (left_item && right_item && all_equal) {
			if (!json_value_equal(left_item, right_item,
			                      opts->strict_equality))
				all_equal = false;
			left_item = left_item->next;
			right_item = right_item->next;
		}

		if (all_equal)
			return NULL;
	}

	cJSON *diff_obj = cJSON_CreateObject();
	if (!diff_obj)
		return NULL;

	char index_str[32];
	bool has_changes = false;
	int min_size = (left_size < right_size) ? left_size : right_size;

	/* Use direct child iteration for better performance */
	cJSON *left_item = left->child;
	cJSON *right_item = right->child;
	int i = 0;

	/* Process common elements */
	while (i < min_size && left_item && right_item) {
		if (!json_value_equal(left_item, right_item,
		                      opts->strict_equality)) {
			/* Check for nested object diff */
			if (cJSON_IsObject(left_item) &&
			    cJSON_IsObject(right_item)) {
				cJSON *sub_diff =
				    json_diff(left_item, right_item, opts);
				if (sub_diff) {
					snprintf_s(index_str, sizeof(index_str), "%d", i);
					cJSON_AddItemToObject(
					    diff_obj, index_str, sub_diff);
					has_changes = true;
				}
			} else {
				/* Simple replacement - create addition and
				 * deletion */
				cJSON *add_array =
				    create_addition_array(right_item);
				cJSON *del_array =
				    create_deletion_array(left_item);
				if (add_array && del_array) {
					/* Addition at index */
					(void)snprintf_s(index_str, sizeof(index_str),
					         "%d", i);
					cJSON_AddItemToObject(
					    diff_obj, index_str, add_array);
					/* Deletion at index */
					snprintf_s(index_str, sizeof(index_str), "_%d", i);
					cJSON_AddItemToObject(
					    diff_obj, index_str, del_array);
					has_changes = true;
				} else {
					if (add_array)
						cJSON_Delete(add_array);
					if (del_array)
						cJSON_Delete(del_array);
				}
			}
		}
		left_item = left_item->next;
		right_item = right_item->next;
		i++;
	}

	/* Handle remaining elements in left (deletions) */
	while (left_item) {
		cJSON *del_array = create_deletion_array(left_item);
		if (del_array) {
			snprintf_s(index_str, sizeof(index_str), "_%d", i);
			cJSON_AddItemToObject(diff_obj, index_str, del_array);
			has_changes = true;
		}
		left_item = left_item->next;
		i++;
	}

	/* Handle remaining elements in right (insertions) */
	while (right_item) {
		cJSON *ins_array = create_addition_array(right_item);
		if (ins_array) {
			snprintf_s(index_str, sizeof(index_str), "%d", i);
			cJSON_AddItemToObject(diff_obj, index_str, ins_array);
			has_changes = true;
		}
		right_item = right_item->next;
		i++;
	}

	if (has_changes) {
		cJSON_AddStringToObject(diff_obj, ARRAY_MARKER,
		                        ARRAY_MARKER_VALUE);
		return diff_obj;
	}

	cJSON_Delete(diff_obj);
	return NULL;
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
	return myers_diff_arrays(left, right, opts);
}

/**
 * do_json_diff - Core implementation of diff without arena setup
 * @left: first JSON value
 * @right: second JSON value
 * @opts: diff options (must not be NULL)
 *
 * Return: diff object or NULL if values are equal
 */
static cJSON *do_json_diff(const cJSON *left, const cJSON *right,
                           const struct json_diff_options *opts)
{
	struct json_diff_options default_opts = {.strict_equality = true};
	cJSON *diff_obj;
	bool has_changes = false;

	if (!opts)
		opts = &default_opts;

	/* Fast path for identical pointers */
	if (left == right)
		return NULL;

	if (json_value_equal(left, right, opts->strict_equality))
		return NULL;

	if (!left || !right || left->type != right->type) {
		/* Simple value change - type mismatch or null values */
		return create_change_array(left, right);
	}

	if (left->type != cJSON_Object && left->type != cJSON_Array) {
		/* Simple value change for non-container types */
		return create_change_array(left, right);
	}

	if (left->type == cJSON_Array)
		return diff_arrays(left, right, opts);

	/* Object diff - use direct child iteration for better performance */
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
			if (del_array) {
				cJSON_AddItemToObject(diff_obj, key, del_array);
				has_changes = true;
			}
		} else {
			/* Key exists in both, check for changes */
			cJSON *sub_diff =
			    json_diff(left_item, right_item, opts);
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
			if (add_array) {
				cJSON_AddItemToObject(diff_obj, key, add_array);
				has_changes = true;
			}
		}
		right_item = right_item->next;
	}

	if (has_changes)
		return diff_obj;

	cJSON_Delete(diff_obj);
	return NULL;
}

/**
 * patch_array - Apply array diff to an array (following Elixir logic)
 * @original: original array
 * @diff: array diff object
 *
 * Return: patched array or NULL on failure
 */
static cJSON *patch_array(const cJSON *original, const cJSON *diff)
{
	cJSON *result = cJSON_CreateArray();
	int orig_size = cJSON_GetArraySize(original);
	int i;

	if (!result)
		return NULL;

	/* First, create a working array with indices */
	cJSON *working_array = cJSON_CreateArray();
	if (!working_array) {
		cJSON_Delete(result);
		return NULL;
	}

	for (i = 0; i < orig_size; i++) {
		cJSON *orig_item = cJSON_GetArrayItem(original, i);
		if (orig_item) {
			cJSON *copy;
			if (cJSON_IsObject(orig_item)) {
				copy = cJSON_CreateObjectReference(orig_item);
			} else if (cJSON_IsArray(orig_item)) {
				copy = cJSON_CreateArrayReference(orig_item);
			} else if (cJSON_IsString(orig_item)) {
				copy = cJSON_CreateString(orig_item->valuestring);
			} else if (cJSON_IsNumber(orig_item)) {
				copy = cJSON_CreateNumber(orig_item->valuedouble);
			} else if (cJSON_IsBool(orig_item)) {
				copy = cJSON_CreateBool(cJSON_IsTrue(orig_item));
			} else {
				copy = cJSON_CreateNull();
			}
			if (copy)
				cJSON_AddItemToArray(working_array, copy);
		}
	}

	/* Apply deletions first (in reverse order to maintain indices) */
	cJSON *diff_item = diff->child;
	int *delete_indices = NULL;
	int delete_count = 0;

	/* Collect deletion indices */
	while (diff_item) {
		const char *key = diff_item->string;
		if (key[0] == '_' && strcmp(key, ARRAY_MARKER) != 0) {
			char *endptr = NULL;
			long index_long = strtol(key + 1, &endptr, 10);
			if (endptr == key + 1 || *endptr != '\0' ||
			    index_long < 0 || index_long > INT_MAX) {
				diff_item = diff_item->next;
				continue;
			}
			int index = (int)index_long;
			int *new_indices =
			    realloc(delete_indices,
			            ((size_t)delete_count + 1) * sizeof(int));
			if (!new_indices) {
				free(delete_indices);
				cJSON_Delete(working_array);
				cJSON_Delete(result);
				return NULL;
			}
			delete_indices = new_indices;
			delete_indices[delete_count] = index;
			delete_count++;
		}
		diff_item = diff_item->next;
	}

	/* Sort deletion indices in descending order */
	for (i = 0; i < delete_count - 1; i++) {
		for (int j = i + 1; j < delete_count; j++) {
			if (delete_indices[i] < delete_indices[j]) {
				int temp = delete_indices[i];
				delete_indices[i] = delete_indices[j];
				delete_indices[j] = temp;
			}
		}
	}

	/* Apply deletions */
	for (i = 0; i < delete_count; i++) {
		int index = delete_indices[i];
		if (index >= 0 && index < cJSON_GetArraySize(working_array)) {
			cJSON_DeleteItemFromArray(working_array, index);
		}
	}
	free(delete_indices);

	/* Now apply additions and modifications */
	diff_item = diff->child;
	while (diff_item) {
		const char *key = diff_item->string;

		if (strcmp(key, ARRAY_MARKER) == 0 || key[0] == '_') {
			diff_item = diff_item->next;
			continue;
		}

		char *endptr = NULL;
		long index_long = strtol(key, &endptr, 10);
		if (endptr == key || *endptr != '\0' || index_long < 0 ||
		    index_long > INT_MAX) {
			diff_item = diff_item->next;
			continue;
		}
		int index = (int)index_long;
		if (cJSON_IsArray(diff_item)) {
			int array_size = cJSON_GetArraySize(diff_item);
			if (array_size == 1) {
				/* Addition */
				cJSON *src_val = cJSON_GetArrayItem(diff_item, 0);
				cJSON *new_val;
				if (cJSON_IsObject(src_val)) {
					new_val = cJSON_CreateObjectReference(src_val);
				} else if (cJSON_IsArray(src_val)) {
					new_val = cJSON_CreateArrayReference(src_val);
				} else if (cJSON_IsString(src_val)) {
					new_val = cJSON_CreateString(src_val->valuestring);
				} else if (cJSON_IsNumber(src_val)) {
					new_val = cJSON_CreateNumber(src_val->valuedouble);
				} else if (cJSON_IsBool(src_val)) {
					new_val = cJSON_CreateBool(cJSON_IsTrue(src_val));
				} else {
					new_val = cJSON_CreateNull();
				}
				if (new_val) {
					int current_size =
					    cJSON_GetArraySize(working_array);
					if (index >= current_size) {
						cJSON_AddItemToArray(
						    working_array, new_val);
					} else if (index >= 0) {
						cJSON_InsertItemInArray(
						    working_array, index,
						    new_val);
					} else {
						cJSON_Delete(new_val);
					}
				}
			} else if (array_size == 2) {
				/* Replacement */
				cJSON *src_val = cJSON_GetArrayItem(diff_item, 1);
				cJSON *new_val;
				if (cJSON_IsObject(src_val)) {
					new_val = cJSON_CreateObjectReference(src_val);
				} else if (cJSON_IsArray(src_val)) {
					new_val = cJSON_CreateArrayReference(src_val);
				} else if (cJSON_IsString(src_val)) {
					new_val = cJSON_CreateString(src_val->valuestring);
				} else if (cJSON_IsNumber(src_val)) {
					new_val = cJSON_CreateNumber(src_val->valuedouble);
				} else if (cJSON_IsBool(src_val)) {
					new_val = cJSON_CreateBool(cJSON_IsTrue(src_val));
				} else {
					new_val = cJSON_CreateNull();
				}
				if (new_val && index >= 0 &&
				    index < cJSON_GetArraySize(working_array)) {
					cJSON_ReplaceItemInArray(
					    working_array, index, new_val);
				} else if (new_val) {
					cJSON_Delete(new_val);
				}
			}
		} else {
			/* Nested diff */
			if (index >= 0 &&
			    index < cJSON_GetArraySize(working_array)) {
				cJSON *orig_val =
				    cJSON_GetArrayItem(working_array, index);
				if (orig_val) {
					cJSON *patched =
					    json_patch(orig_val, diff_item);
					if (patched) {
						cJSON_ReplaceItemInArray(
						    working_array, index,
						    patched);
					}
				}
			}
		}
		diff_item = diff_item->next;
	}

	/* Copy final result */
	int final_size = cJSON_GetArraySize(working_array);
	for (i = 0; i < final_size; i++) {
		cJSON *item = cJSON_GetArrayItem(working_array, i);
		if (item) {
			cJSON *copy;
			if (cJSON_IsObject(item)) {
				copy = cJSON_CreateObjectReference(item);
			} else if (cJSON_IsArray(item)) {
				copy = cJSON_CreateArrayReference(item);
			} else if (cJSON_IsString(item)) {
				copy = cJSON_CreateString(item->valuestring);
			} else if (cJSON_IsNumber(item)) {
				copy = cJSON_CreateNumber(item->valuedouble);
			} else if (cJSON_IsBool(item)) {
				copy = cJSON_CreateBool(cJSON_IsTrue(item));
			} else {
				copy = cJSON_CreateNull();
			}
			if (copy)
				cJSON_AddItemToArray(result, copy);
		}
	}

	cJSON_Delete(working_array);
	return result;
}

/**
 * json_diff - Public wrapper that sets up optional arena before diff
 * @left: first JSON value
 * @right: second JSON value
 * @opts: diff options (can be NULL for defaults, opts->arena may be used)
 *
 * Return: diff object or NULL if values are equal
 */
cJSON *json_diff(const cJSON *left, const cJSON *right,
                 const struct json_diff_options *opts)
{
	struct json_diff_options default_opts = {.strict_equality = true,
	                                         .arena = NULL};
	if (!opts)
		opts = &default_opts;
	if (opts->arena) {
		current_arena = opts->arena;
		opts->arena->offset = 0;
		cJSON_Hooks h = {arena_malloc, arena_free};
		cJSON_InitHooks(&h);
	}

	cJSON *res = do_json_diff(left, right, opts);

	if (opts->arena) {
		cJSON_Hooks h = {malloc, free};
		cJSON_InitHooks(&h);
		current_arena = NULL;
	}
	return res;
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

	/* Handle simple value replacement (type changes) */
	if (cJSON_IsArray(diff) && cJSON_GetArraySize(diff) == 2) {
		/* This is a change array [old_value, new_value] */
		cJSON *new_val = cJSON_GetArrayItem(diff, 1);
		if (cJSON_IsObject(new_val)) {
			return cJSON_CreateObjectReference(new_val);
		} else if (cJSON_IsArray(new_val)) {
			return cJSON_CreateArrayReference(new_val);
		} else if (cJSON_IsString(new_val)) {
			return cJSON_CreateString(new_val->valuestring);
		} else if (cJSON_IsNumber(new_val)) {
			return cJSON_CreateNumber(new_val->valuedouble);
		} else if (cJSON_IsBool(new_val)) {
			return cJSON_CreateBool(cJSON_IsTrue(new_val));
		} else {
			return cJSON_CreateNull();
		}
	}

	if (!cJSON_IsObject(diff)) {
		if (cJSON_IsObject(original)) {
			return cJSON_CreateObjectReference(original);
		} else if (cJSON_IsArray(original)) {
			return cJSON_CreateArrayReference(original);
		} else if (cJSON_IsString(original)) {
			return cJSON_CreateString(original->valuestring);
		} else if (cJSON_IsNumber(original)) {
			return cJSON_CreateNumber(original->valuedouble);
		} else if (cJSON_IsBool(original)) {
			return cJSON_CreateBool(cJSON_IsTrue(original));
		} else {
			return cJSON_CreateNull();
		}
	}

	/* Check if this is an array diff */
	if (cJSON_GetObjectItem(diff, ARRAY_MARKER)) {
		if (cJSON_IsArray(original)) {
			return patch_array(original, diff);
		} else {
			if (cJSON_IsObject(original)) {
				return cJSON_CreateObjectReference(original);
			} else if (cJSON_IsArray(original)) {
				return cJSON_CreateArrayReference(original);
			} else if (cJSON_IsString(original)) {
				return cJSON_CreateString(original->valuestring);
			} else if (cJSON_IsNumber(original)) {
				return cJSON_CreateNumber(original->valuedouble);
			} else if (cJSON_IsBool(original)) {
				return cJSON_CreateBool(cJSON_IsTrue(original));
			} else {
				return cJSON_CreateNull();
			}
		}
	}

	/* For non-object originals with object diffs, create new object */
	if (!cJSON_IsObject(original)) {
		result = cJSON_CreateObject();
	} else {
		/* Copy original object structure */
		result = cJSON_CreateObject();
		cJSON *orig_item = original->child;
		while (orig_item) {
			cJSON *copy_item;
			if (cJSON_IsObject(orig_item)) {
				copy_item = cJSON_CreateObjectReference(orig_item);
			} else if (cJSON_IsArray(orig_item)) {
				copy_item = cJSON_CreateArrayReference(orig_item);
			} else if (cJSON_IsString(orig_item)) {
				copy_item = cJSON_CreateString(orig_item->valuestring);
			} else if (cJSON_IsNumber(orig_item)) {
				copy_item = cJSON_CreateNumber(orig_item->valuedouble);
			} else if (cJSON_IsBool(orig_item)) {
				copy_item = cJSON_CreateBool(cJSON_IsTrue(orig_item));
			} else {
				copy_item = cJSON_CreateNull();
			}
			if (copy_item) {
				cJSON_AddItemToObject(result, orig_item->string, copy_item);
			}
			orig_item = orig_item->next;
		}
	}
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
				cJSON *src_val = cJSON_GetArrayItem(diff_item, 0);
				cJSON *new_val;
				if (cJSON_IsObject(src_val)) {
					new_val = cJSON_CreateObjectReference(src_val);
				} else if (cJSON_IsArray(src_val)) {
					new_val = cJSON_CreateArrayReference(src_val);
				} else if (cJSON_IsString(src_val)) {
					new_val = cJSON_CreateString(src_val->valuestring);
				} else if (cJSON_IsNumber(src_val)) {
					new_val = cJSON_CreateNumber(src_val->valuedouble);
				} else if (cJSON_IsBool(src_val)) {
					new_val = cJSON_CreateBool(cJSON_IsTrue(src_val));
				} else {
					new_val = cJSON_CreateNull();
				}
				if (new_val) {
					cJSON_DeleteItemFromObject(result, key);
					cJSON_AddItemToObject(result, key, new_val);
				}
			} else if (array_size == 3) {
				/* Deletion - remove key */
				cJSON_DeleteItemFromObject(result, key);
			} else if (array_size == 2) {
				/* Replacement */
				cJSON *src_val = cJSON_GetArrayItem(diff_item, 1);
				cJSON *new_val;
				if (cJSON_IsObject(src_val)) {
					new_val = cJSON_CreateObjectReference(src_val);
				} else if (cJSON_IsArray(src_val)) {
					new_val = cJSON_CreateArrayReference(src_val);
				} else if (cJSON_IsString(src_val)) {
					new_val = cJSON_CreateString(src_val->valuestring);
				} else if (cJSON_IsNumber(src_val)) {
					new_val = cJSON_CreateNumber(src_val->valuedouble);
				} else if (cJSON_IsBool(src_val)) {
					new_val = cJSON_CreateBool(cJSON_IsTrue(src_val));
				} else {
					new_val = cJSON_CreateNull();
				}
				if (new_val) {
					cJSON_DeleteItemFromObject(result, key);
					cJSON_AddItemToObject(result, key, new_val);
				}
			}
		} else {
			/* Nested diff */
			cJSON *orig_val = cJSON_GetObjectItem(result, key);
			if (orig_val) {
				cJSON *patched =
				    json_patch(orig_val, diff_item);
				if (patched) {
					cJSON_DeleteItemFromObject(result, key);
					cJSON_AddItemToObject(result, key,
					                      patched);
				}
			}
		}
		diff_item = diff_item->next;
	}

	return result;
}

cJSON *json_diff_str(const char *left, const char *right,
                     const struct json_diff_options *opts)
{
	if (!left || !right)
		return NULL;

	cJSON *left_json = cJSON_Parse(left);
	if (!left_json)
		return NULL;

	cJSON *right_json = cJSON_Parse(right);
	if (!right_json) {
		cJSON_Delete(left_json);
		return NULL;
	}

	cJSON *diff = json_diff(left_json, right_json, opts);

	cJSON_Delete(left_json);
	cJSON_Delete(right_json);

	return diff;
}
