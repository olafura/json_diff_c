// SPDX-License-Identifier: Apache-2.0
#define __STDC_WANT_LIB_EXT1__ 1
#include "json_diff.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Arena-based allocation hooks for diff trees */
#include <pthread.h>

#ifndef MAX_JSON_DEPTH
#define MAX_JSON_DEPTH 1024
#endif
/* Arena-based allocation hooks for diff trees */
static __thread struct json_diff_arena *current_arena = NULL;
static __thread int json_diff_depth = 0;
static __thread int json_patch_depth = 0;

#ifndef MAX_JSON_DIFF_DEPTH
#define MAX_JSON_DIFF_DEPTH 1024
#endif
#ifndef MAX_JSON_INPUT_SIZE
#define MAX_JSON_INPUT_SIZE (1024 * 1024)
#endif

/* Create a shallow clone of a value:
 * - Objects/arrays: new container with children added as references
 * - Primitives: new primitive with same value
 */
static cJSON *clone_shallow(const cJSON *v)
{
	if (!v)
		return cJSON_CreateNull();
	if (cJSON_IsObject(v)) {
		cJSON *o = cJSON_CreateObject();
		if (!o)
			return NULL;
		for (const cJSON *ch = v->child; ch; ch = ch->next) {
			cJSON *val = NULL;
			if (cJSON_IsObject(ch))
				val = cJSON_CreateObjectReference(ch);
			else if (cJSON_IsArray(ch))
				val = cJSON_CreateArrayReference(ch);
			else if (cJSON_IsString(ch))
				val = cJSON_CreateString(ch->valuestring);
			else if (cJSON_IsNumber(ch))
				val = cJSON_CreateNumber(ch->valuedouble);
			else if (cJSON_IsBool(ch))
				val = cJSON_CreateBool(cJSON_IsTrue(ch));
			else
				val = cJSON_CreateNull();
			if (!val) {
				cJSON_Delete(o);
				return NULL;
			}
			cJSON_AddItemToObject(o, ch->string ? ch->string : "",
			                      val);
		}
		return o;
	}
	if (cJSON_IsArray(v)) {
		cJSON *a = cJSON_CreateArray();
		if (!a)
			return NULL;
		for (const cJSON *ch = v->child; ch; ch = ch->next) {
			cJSON *val = NULL;
			if (cJSON_IsObject(ch))
				val = cJSON_CreateObjectReference(ch);
			else if (cJSON_IsArray(ch))
				val = cJSON_CreateArrayReference(ch);
			else if (cJSON_IsString(ch))
				val = cJSON_CreateString(ch->valuestring);
			else if (cJSON_IsNumber(ch))
				val = cJSON_CreateNumber(ch->valuedouble);
			else if (cJSON_IsBool(ch))
				val = cJSON_CreateBool(cJSON_IsTrue(ch));
			else
				val = cJSON_CreateNull();
			if (!val) {
				cJSON_Delete(a);
				return NULL;
			}
			cJSON_AddItemToArray(a, val);
		}
		return a;
	}
	if (cJSON_IsString(v))
		return cJSON_CreateString(v->valuestring);
	if (cJSON_IsNumber(v))
		return cJSON_CreateNumber(v->valuedouble);
	if (cJSON_IsBool(v))
		return cJSON_CreateBool(cJSON_IsTrue(v));
	return cJSON_CreateNull();
}

static void *arena_malloc(size_t size)
{
	if (!current_arena)
		return NULL;
	/* Prevent overflow in offset rounding */
	if (current_arena->offset > SIZE_MAX - (sizeof(void *) - 1))
		return NULL;
	size_t off = (current_arena->offset + sizeof(void *) - 1) &
	             ~(sizeof(void *) - 1);
	/* Prevent overflow in allocation size */
	if (off > SIZE_MAX - size)
		return NULL;
	if (off + size > current_arena->capacity) {
		size_t newcap = current_arena->capacity
		                    ? current_arena->capacity * 2
		                    : size * 2;
		if (newcap < size)
			newcap = size * 2;
		/* Prevent excessive memory usage */
		if (newcap > MAX_ARENA_SIZE)
			return NULL;
		while (newcap < off + size) {
			if (newcap > SIZE_MAX / 2 || newcap > MAX_ARENA_SIZE)
				return NULL;
			newcap *= 2;
			if (newcap > MAX_ARENA_SIZE)
				return NULL;
		}
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
	if (!arena->buf) {
		arena->capacity = arena->offset = 0;
	} else {
		arena->capacity = initial_capacity;
		arena->offset = 0;
	}
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
	if (!old_val) {
		old_item = cJSON_CreateNull();
	} else if (cJSON_IsObject(old_val)) {
		old_item = clone_shallow(old_val);
	} else if (cJSON_IsArray(old_val)) {
		old_item = clone_shallow(old_val);
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
	if (!new_val) {
		new_item = cJSON_CreateNull();
	} else if (cJSON_IsObject(new_val)) {
		new_item = clone_shallow(new_val);
	} else if (cJSON_IsArray(new_val)) {
		new_item = clone_shallow(new_val);
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
		new_item = clone_shallow(new_val);
	} else if (cJSON_IsArray(new_val)) {
		new_item = clone_shallow(new_val);
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
		old_item = cJSON_Duplicate(old_val, 1);
	} else if (cJSON_IsArray(old_val)) {
		old_item = cJSON_Duplicate(old_val, 1);
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

	/* Quick equality check */
	if (left_size == right_size) {
		bool all_equal = true;
		for (int k = 0; k < left_size; k++) {
			cJSON *li = cJSON_GetArrayItem(left, k);
			cJSON *ri = cJSON_GetArrayItem(right, k);
			if (!json_value_equal(li, ri, opts->strict_equality)) {
				all_equal = false;
				break;
			}
		}
		if (all_equal)
			return NULL;
	}

	cJSON *diff_obj = cJSON_CreateObject();
	if (!diff_obj)
		return NULL;

	bool has_changes = false;
	int i = 0, j = 0; /* indices for left and right */
	char index_str[32];

	while (i < left_size && j < right_size) {
		cJSON *li = cJSON_GetArrayItem(left, i);
		cJSON *ri = cJSON_GetArrayItem(right, j);

		if (json_value_equal(li, ri, opts->strict_equality)) {
			i++;
			j++;
			continue;
		}

		/* Lookahead: deletion */
		if ((i + 1) < left_size) {
			cJSON *li1 = cJSON_GetArrayItem(left, i + 1);
			if (json_value_equal(li1, ri, opts->strict_equality)) {
				/* Deletion of left[i] */
#ifdef __STDC_LIB_EXT1__
				snprintf_s(index_str, sizeof(index_str), "_%d",
				           i);
#else
				snprintf(index_str, sizeof(index_str), "_%d",
				         i);
#endif
				cJSON *del_array = create_deletion_array(li);
				if (del_array) {
					cJSON_AddItemToObject(
					    diff_obj, index_str, del_array);
					has_changes = true;
				}
				i++;
				continue;
			}
		}

		/* Lookahead: insertion */
		if ((j + 1) < right_size) {
			cJSON *rj1 = cJSON_GetArrayItem(right, j + 1);
			if (json_value_equal(li, rj1, opts->strict_equality)) {
				/* Insertion of right[j] at i */
#ifdef __STDC_LIB_EXT1__
				snprintf_s(index_str, sizeof(index_str), "%d",
				           i);
#else
				snprintf(index_str, sizeof(index_str), "%d", i);
#endif
				cJSON *ins_array = create_addition_array(ri);
				if (ins_array) {
					cJSON_AddItemToObject(
					    diff_obj, index_str, ins_array);
					has_changes = true;
				}
				j++;
				continue;
			}
		}

		/* Replacement: either nested diff for objects or add+del pair
		 */
		if (cJSON_IsObject(li) && cJSON_IsObject(ri)) {
			cJSON *sub = json_diff(li, ri, opts);
			if (sub) {
#ifdef __STDC_LIB_EXT1__
				snprintf_s(index_str, sizeof(index_str), "%d",
				           i);
#else
				snprintf(index_str, sizeof(index_str), "%d", i);
#endif
				cJSON_AddItemToObject(diff_obj, index_str, sub);
				has_changes = true;
			}
		} else {
			cJSON *add_array = create_addition_array(ri);
			cJSON *del_array = create_deletion_array(li);
			if (add_array && del_array) {
#ifdef __STDC_LIB_EXT1__
				(void)snprintf_s(index_str, sizeof(index_str),
				                 "%d", i);
#else
				(void)snprintf(index_str, sizeof(index_str),
				               "%d", i);
#endif
				cJSON_AddItemToObject(diff_obj, index_str,
				                      add_array);
#ifdef __STDC_LIB_EXT1__
				snprintf_s(index_str, sizeof(index_str), "_%d",
				           i);
#else
				snprintf(index_str, sizeof(index_str), "_%d",
				         i);
#endif
				cJSON_AddItemToObject(diff_obj, index_str,
				                      del_array);
				has_changes = true;
			} else {
				if (add_array)
					cJSON_Delete(add_array);
				if (del_array)
					cJSON_Delete(del_array);
			}
		}

		i++;
		j++;
	}

	/* Remaining deletions */
	while (i < left_size) {
		cJSON *li = cJSON_GetArrayItem(left, i);
		cJSON *del_array = create_deletion_array(li);
		if (del_array) {
#ifdef __STDC_LIB_EXT1__
			snprintf_s(index_str, sizeof(index_str), "_%d", i);
#else
			snprintf(index_str, sizeof(index_str), "_%d", i);
#endif
			cJSON_AddItemToObject(diff_obj, index_str, del_array);
			has_changes = true;
		}
		i++;
	}

	/* Remaining insertions */
	while (j < right_size) {
		cJSON *ri = cJSON_GetArrayItem(right, j);
		cJSON *ins_array = create_addition_array(ri);
		if (ins_array) {
#ifdef __STDC_LIB_EXT1__
			snprintf_s(index_str, sizeof(index_str), "%d", i);
#else
			snprintf(index_str, sizeof(index_str), "%d", i);
#endif
			cJSON_AddItemToObject(diff_obj, index_str, ins_array);
			has_changes = true;
		}
		j++;
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
 * Return: diff object or NULL if values are equal or on error
 */
static cJSON *do_json_diff(const cJSON *left, const cJSON *right,
                           const struct json_diff_options *opts)
{
	cJSON *result = NULL;
	struct json_diff_options default_opts = {.strict_equality = true};
	if (!opts)
		opts = &default_opts;

	/* Recursion depth guard */
	if (++json_diff_depth > MAX_JSON_DEPTH)
		return NULL;

	/* Fast path for identical pointers or equal values */
	if (left == right ||
	    json_value_equal(left, right, opts->strict_equality))
		goto finish;

	/* Simple type or null mismatch */
	if (!left || !right || left->type != right->type) {
		result = create_change_array(left, right);
		goto finish;
	}

	/* Simple non-container types */
	if (left->type != cJSON_Object && left->type != cJSON_Array) {
		result = create_change_array(left, right);
		goto finish;
	}

	/* Array diff */
	if (left->type == cJSON_Array) {
		result = diff_arrays(left, right, opts);
		goto finish;
	}

	/* Object diff */
	cJSON *diff_obj = cJSON_CreateObject();
	if (!diff_obj)
		goto finish;
	{
		bool has_changes = false;
		for (const cJSON *li = left->child; li; li = li->next) {
			const char *key = li->string;
			cJSON *ri = cJSON_GetObjectItem(right, key);
			if (!ri) {
				cJSON *d = create_deletion_array(li);
				if (d) {
					cJSON_AddItemToObject(diff_obj, key, d);
					has_changes = true;
				}
			} else {
				cJSON *sd = json_diff(li, ri, opts);
				if (sd) {
					cJSON_AddItemToObject(diff_obj, key,
					                      sd);
					has_changes = true;
				}
			}
		}
		for (const cJSON *ri = right->child; ri; ri = ri->next) {
			const char *key = ri->string;
			if (!cJSON_GetObjectItem(left, key)) {
				cJSON *a = create_addition_array(ri);
				if (a) {
					cJSON_AddItemToObject(diff_obj, key, a);
					has_changes = true;
				}
			}
		}
		if (has_changes)
			result = diff_obj;
		else
			cJSON_Delete(diff_obj);
	}

finish:
	--json_diff_depth;
	return result;
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
	/* Defensive: if we were accidentally handed an object whose only child
	 * value is an array, unwrap it so we operate on the array itself. */
	if (!cJSON_IsArray(original) && cJSON_IsObject(original) &&
	    original->child && original->child->next == NULL &&
	    cJSON_IsArray(original->child)) {
		original = original->child;
	}

	/* Unwrap array-reference style wrapper: sometimes an array reference is
	 * represented as an array whose single element is the target array. */
	if (cJSON_IsArray(original) && cJSON_GetArraySize(original) == 1) {
		cJSON *only = cJSON_GetArrayItem(original, 0);
		if (only && cJSON_IsArray(only)) {
			original = only;
		}
	}

	cJSON *result = cJSON_CreateArray();
	int i;

	if (!result)
		return NULL;

	/* Start from a deep copy of the original array to avoid aliasing */
	cJSON *working_array = cJSON_Duplicate(original, 1);
	if (!working_array) {
		cJSON_Delete(result);
		return NULL;
	}

	/* First pass: record indices that are additions with a single value.
	 * These are replacements when paired with a deletion of the same index.
	 */
	int *replace_indices = NULL;
	int replace_count = 0;
	for (cJSON *it = diff->child; it; it = it->next) {
		const char *k = it->string;
		if (!k || k[0] == '_' || strcmp(k, ARRAY_MARKER) == 0)
			continue;
		if (!cJSON_IsArray(it) || cJSON_GetArraySize(it) != 1)
			continue;
		char *ep = NULL;
		long idx = strtol(k, &ep, 10);
		if (ep == k || *ep != '\0' || idx < 0 || idx > INT_MAX)
			continue;
		int *tmp =
		    realloc(replace_indices, (replace_count + 1) * sizeof(int));
		if (!tmp) {
			free(replace_indices);
			cJSON_Delete(working_array);
			cJSON_Delete(result);
			return NULL;
		}
		replace_indices = tmp;
		replace_indices[replace_count++] = (int)idx;
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
			/* Safe cast after bounds check */
			int index = (int)index_long;
			/* Skip deletion if we also have a single-element
			 * addition at same index */
			bool skip = false;
			for (int r = 0; r < replace_count; r++) {
				if (replace_indices[r] == index) {
					skip = true;
					break;
				}
			}
			if (skip) {
				diff_item = diff_item->next;
				continue;
			}
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
	free(replace_indices);

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
		/* Safe cast after bounds check */
		int index = (int)index_long;
		if (cJSON_IsArray(diff_item)) {
			int array_size = cJSON_GetArraySize(diff_item);
			if (array_size == 1) {
				/* Addition */
				cJSON *src_val =
				    cJSON_GetArrayItem(diff_item, 0);
				cJSON *new_val;
				if (cJSON_IsObject(src_val)) {
					new_val = cJSON_Duplicate(src_val, 1);
				} else if (cJSON_IsArray(src_val)) {
					new_val = cJSON_Duplicate(src_val, 1);
				} else if (cJSON_IsString(src_val)) {
					new_val = cJSON_CreateString(
					    src_val->valuestring);
				} else if (cJSON_IsNumber(src_val)) {
					new_val = cJSON_CreateNumber(
					    src_val->valuedouble);
				} else if (cJSON_IsBool(src_val)) {
					new_val = cJSON_CreateBool(
					    cJSON_IsTrue(src_val));
				} else {
					new_val = cJSON_CreateNull();
				}
				if (new_val) {
					int current_size =
					    cJSON_GetArraySize(working_array);
					if (index >= 0 &&
					    index < current_size) {
						/* Treat as replacement if
						 * within bounds */
						cJSON_ReplaceItemInArray(
						    working_array, index,
						    new_val);
					} else if (index >= current_size) {
						cJSON_AddItemToArray(
						    working_array, new_val);
					} else {
						cJSON_Delete(new_val);
					}
				}
			} else if (array_size == 2) {
				/* Replacement */
				cJSON *src_val =
				    cJSON_GetArrayItem(diff_item, 1);
				cJSON *new_val;
				if (cJSON_IsObject(src_val)) {
					new_val = cJSON_Duplicate(src_val, 1);
				} else if (cJSON_IsArray(src_val)) {
					new_val = cJSON_Duplicate(src_val, 1);
				} else if (cJSON_IsString(src_val)) {
					new_val = cJSON_CreateString(
					    src_val->valuestring);
				} else if (cJSON_IsNumber(src_val)) {
					new_val = cJSON_CreateNumber(
					    src_val->valuedouble);
				} else if (cJSON_IsBool(src_val)) {
					new_val = cJSON_CreateBool(
					    cJSON_IsTrue(src_val));
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

	/* We can return the working array directly */
	cJSON_Delete(result);
	return working_array;
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
	if (++json_diff_depth > MAX_JSON_DEPTH) {
		--json_diff_depth;
		return NULL;
	}
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
	--json_diff_depth;
	return res;
}
/**
 * json_patch - Apply a diff to a cJSON value
 * @original: original JSON value
 * @diff: diff to apply
 *
 * Return: patched JSON value or NULL on failure
 */
/// Apply a diff; bail out on excessive recursion
cJSON *json_patch(const cJSON *original, const cJSON *diff)
{
	if (++json_patch_depth > MAX_JSON_DEPTH)
		return NULL;
	cJSON *result = NULL;

	if (!original || !diff)
		goto out;

	/* Handle simple value replacement (type changes) */
	if (cJSON_IsArray(diff) && cJSON_GetArraySize(diff) == 2) {
		/* This is a change array [old_value, new_value] */
		cJSON *new_val = cJSON_GetArrayItem(diff, 1);
		if (cJSON_IsObject(new_val)) {
			/* Handle object reference wrapper - if it has only one
			 * child with empty key, unwrap it */
			if (new_val->child && !new_val->child->next &&
			    new_val->child->string &&
			    strlen(new_val->child->string) == 0) {
				return cJSON_CreateObjectReference(
				    new_val->child);
			}
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
				return cJSON_CreateString(
				    original->valuestring);
			} else if (cJSON_IsNumber(original)) {
				return cJSON_CreateNumber(
				    original->valuedouble);
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
				copy_item =
				    cJSON_CreateObjectReference(orig_item);
			} else if (cJSON_IsArray(orig_item)) {
				copy_item =
				    cJSON_CreateArrayReference(orig_item);
			} else if (cJSON_IsString(orig_item)) {
				copy_item =
				    cJSON_CreateString(orig_item->valuestring);
			} else if (cJSON_IsNumber(orig_item)) {
				copy_item =
				    cJSON_CreateNumber(orig_item->valuedouble);
			} else if (cJSON_IsBool(orig_item)) {
				copy_item =
				    cJSON_CreateBool(cJSON_IsTrue(orig_item));
			} else {
				copy_item = cJSON_CreateNull();
			}
			if (copy_item) {
				cJSON_AddItemToObject(result, orig_item->string,
				                      copy_item);
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
				/* Addition or replacement */
				cJSON *src_val =
				    cJSON_GetArrayItem(diff_item, 0);
				cJSON *new_val;
				if (cJSON_IsObject(src_val)) {
					new_val = cJSON_Duplicate(src_val, 1);
				} else if (cJSON_IsArray(src_val)) {
					new_val = cJSON_Duplicate(src_val, 1);
				} else if (cJSON_IsString(src_val)) {
					new_val = cJSON_CreateString(
					    src_val->valuestring);
				} else if (cJSON_IsNumber(src_val)) {
					new_val = cJSON_CreateNumber(
					    src_val->valuedouble);
				} else if (cJSON_IsBool(src_val)) {
					new_val = cJSON_CreateBool(
					    cJSON_IsTrue(src_val));
				} else {
					new_val = cJSON_CreateNull();
				}
				if (new_val) {
					cJSON_DeleteItemFromObject(result, key);
					cJSON_AddItemToObject(result, key,
					                      new_val);
				}
			} else if (array_size == 3) {
				/* Deletion - remove key */
				cJSON_DeleteItemFromObject(result, key);
			} else if (array_size == 2) {
				/* Replacement */
				cJSON *src_val =
				    cJSON_GetArrayItem(diff_item, 1);
				cJSON *new_val;
				if (cJSON_IsObject(src_val)) {
					new_val = cJSON_Duplicate(src_val, 1);
				} else if (cJSON_IsArray(src_val)) {
					new_val = cJSON_Duplicate(src_val, 1);
				} else if (cJSON_IsString(src_val)) {
					new_val = cJSON_CreateString(
					    src_val->valuestring);
				} else if (cJSON_IsNumber(src_val)) {
					new_val = cJSON_CreateNumber(
					    src_val->valuedouble);
				} else if (cJSON_IsBool(src_val)) {
					new_val = cJSON_CreateBool(
					    cJSON_IsTrue(src_val));
				} else {
					new_val = cJSON_CreateNull();
				}
				if (new_val) {
					cJSON_DeleteItemFromObject(result, key);
					cJSON_AddItemToObject(result, key,
					                      new_val);
				}
			}
		} else {
			/* Nested diff */
			cJSON *orig_val = cJSON_GetObjectItem(result, key);
			/* Unwrap cJSON reference wrapper objects to real item
			 * when present */
			if (orig_val && (orig_val->type & cJSON_IsReference) &&
			    orig_val->child) {
				orig_val = orig_val->child;
			}
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

out:
	--json_patch_depth;
	return result;
}

cJSON *json_diff_str(const char *left, const char *right,
                     const struct json_diff_options *opts)
{
	/* Reject excessively large inputs to avoid DoS */
	if (!left || !right || strlen(left) > MAX_JSON_INPUT_SIZE ||
	    strlen(right) > MAX_JSON_INPUT_SIZE)
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
