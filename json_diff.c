// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE
#include <math.h>
#include "json_diff.h"

#define INITIAL_CAPACITY 8
#define ARRAY_MARKER "_t"
#define ARRAY_MARKER_VALUE "a"

/* Forward declaration */
static void json_value_free_contents(struct json_value *value);

/**
 * json_value_create_null - Create a null JSON value
 *
 * Return: pointer to new JSON value or NULL on failure
 */
struct json_value *json_value_create_null(void)
{
	struct json_value *value = malloc(sizeof(*value));

	if (!value)
		return NULL;

	value->type = JSON_NULL;
	return value;
}

/**
 * json_value_create_bool - Create a boolean JSON value
 * @val: boolean value
 *
 * Return: pointer to new JSON value or NULL on failure
 */
struct json_value *json_value_create_bool(bool val)
{
	struct json_value *value = malloc(sizeof(*value));

	if (!value)
		return NULL;

	value->type = JSON_BOOL;
	value->data.bool_val = val;
	return value;
}

/**
 * json_value_create_number - Create a numeric JSON value
 * @val: numeric value
 *
 * Return: pointer to new JSON value or NULL on failure
 */
struct json_value *json_value_create_number(double val)
{
	struct json_value *value = malloc(sizeof(*value));

	if (!value)
		return NULL;

	value->type = JSON_NUMBER;
	value->data.number_val = val;
	return value;
}

/**
 * json_value_create_string - Create a string JSON value
 * @val: string value
 *
 * Return: pointer to new JSON value or NULL on failure
 */
struct json_value *json_value_create_string(const char *val)
{
	struct json_value *value = malloc(sizeof(*value));
	char *str_copy;

	if (!value)
		return NULL;

	str_copy = strdup(val);
	if (!str_copy) {
		free(value);
		return NULL;
	}

	value->type = JSON_STRING;
	value->data.string_val = str_copy;
	return value;
}

/**
 * json_value_create_array - Create an array JSON value
 *
 * Return: pointer to new JSON value or NULL on failure
 */
struct json_value *json_value_create_array(void)
{
	struct json_value *value = malloc(sizeof(*value));
	struct json_array *array;

	if (!value)
		return NULL;

	array = malloc(sizeof(*array));
	if (!array) {
		free(value);
		return NULL;
	}

	array->values = malloc(INITIAL_CAPACITY * sizeof(struct json_value));
	if (!array->values) {
		free(array);
		free(value);
		return NULL;
	}

	array->count = 0;
	array->capacity = INITIAL_CAPACITY;

	value->type = JSON_ARRAY;
	value->data.array_val = array;
	return value;
}

/**
 * json_value_create_object - Create an object JSON value
 *
 * Return: pointer to new JSON value or NULL on failure
 */
struct json_value *json_value_create_object(void)
{
	struct json_value *value = malloc(sizeof(*value));
	struct json_object *object;

	if (!value)
		return NULL;

	object = malloc(sizeof(*object));
	if (!object) {
		free(value);
		return NULL;
	}

	object->pairs = malloc(INITIAL_CAPACITY * sizeof(struct json_object_pair));
	if (!object->pairs) {
		free(object);
		free(value);
		return NULL;
	}

	object->count = 0;
	object->capacity = INITIAL_CAPACITY;

	value->type = JSON_OBJECT;
	value->data.object_val = object;
	return value;
}

/**
 * json_array_append - Append a value to a JSON array
 * @array: target array
 * @value: value to append
 *
 * Return: 0 on success, negative on failure
 */
int json_array_append(struct json_array *array, const struct json_value *value)
{
	struct json_value *new_values;
	size_t new_capacity;

	if (array->count >= array->capacity) {
		new_capacity = array->capacity * 2;
		new_values = realloc(array->values,
				     new_capacity * sizeof(struct json_value));
		if (!new_values)
			return -1;

		array->values = new_values;
		array->capacity = new_capacity;
	}

	struct json_value *cloned_value = json_value_clone(value);
	if (!cloned_value)
		return -1;
	
	array->values[array->count] = *cloned_value;
	free(cloned_value); /* Free the wrapper, keep the contents */
	array->count++;
	return 0;
}

/**
 * json_object_set - Set a key-value pair in a JSON object
 * @object: target object
 * @key: object key
 * @value: object value
 *
 * Return: 0 on success, negative on failure
 */
int json_object_set(struct json_object *object, const char *key,
		    const struct json_value *value)
{
	struct json_object_pair *new_pairs;
	size_t new_capacity;
	char *key_copy;
	size_t i;

	/* Check if key already exists */
	for (i = 0; i < object->count; i++) {
		if (strcmp(object->pairs[i].key, key) == 0) {
			json_value_free_contents(&object->pairs[i].value);
			struct json_value *cloned_value = json_value_clone(value);
			if (!cloned_value)
				return -1;
			object->pairs[i].value = *cloned_value;
			free(cloned_value); /* Free the wrapper, keep the contents */
			return 0;
		}
	}

	/* Add new key-value pair */
	if (object->count >= object->capacity) {
		new_capacity = object->capacity * 2;
		new_pairs = realloc(object->pairs,
				    new_capacity * sizeof(struct json_object_pair));
		if (!new_pairs)
			return -1;

		object->pairs = new_pairs;
		object->capacity = new_capacity;
	}

	key_copy = strdup(key);
	if (!key_copy)
		return -1;

	struct json_value *cloned_value = json_value_clone(value);
	if (!cloned_value) {
		free(key_copy);
		return -1;
	}
	
	object->pairs[object->count].key = key_copy;
	object->pairs[object->count].value = *cloned_value;
	free(cloned_value); /* Free the wrapper, keep the contents */
	object->count++;
	return 0;
}

/**
 * json_object_get - Get a value from a JSON object by key
 * @object: source object
 * @key: object key
 *
 * Return: pointer to value or NULL if not found
 */
struct json_value *json_object_get(const struct json_object *object,
				   const char *key)
{
	size_t i;

	for (i = 0; i < object->count; i++) {
		if (strcmp(object->pairs[i].key, key) == 0)
			return &object->pairs[i].value;
	}

	return NULL;
}

/**
 * json_value_equal - Compare two JSON values for equality
 * @left: first value
 * @right: second value
 * @strict: use strict equality for numbers
 *
 * Return: true if equal, false otherwise
 */
bool json_value_equal(const struct json_value *left,
		      const struct json_value *right,
		      bool strict)
{
	size_t i;

	if (left->type != right->type)
		return false;

	switch (left->type) {
	case JSON_NULL:
		return true;
	case JSON_BOOL:
		return left->data.bool_val == right->data.bool_val;
	case JSON_NUMBER:
		if (strict)
			return left->data.number_val == right->data.number_val;
		else
			return fabs(left->data.number_val - right->data.number_val) < 1e-9;
	case JSON_STRING:
		return strcmp(left->data.string_val, right->data.string_val) == 0;
	case JSON_ARRAY:
		if (left->data.array_val->count != right->data.array_val->count)
			return false;
		for (i = 0; i < left->data.array_val->count; i++) {
			if (!json_value_equal(&left->data.array_val->values[i],
					      &right->data.array_val->values[i],
					      strict))
				return false;
		}
		return true;
	case JSON_OBJECT:
		if (left->data.object_val->count != right->data.object_val->count)
			return false;
		for (i = 0; i < left->data.object_val->count; i++) {
			struct json_value *right_val = json_object_get(
				right->data.object_val,
				left->data.object_val->pairs[i].key);
			if (!right_val ||
			    !json_value_equal(&left->data.object_val->pairs[i].value,
					      right_val, strict))
				return false;
		}
		return true;
	}

	return false;
}

/**
 * json_value_clone - Create a deep copy of a JSON value
 * @value: value to clone
 *
 * Return: pointer to cloned value or NULL on failure
 */
struct json_value *json_value_clone(const struct json_value *value)
{
	struct json_value *clone;
	size_t i;

	if (!value)
		return NULL;

	switch (value->type) {
	case JSON_NULL:
		return json_value_create_null();
	case JSON_BOOL:
		return json_value_create_bool(value->data.bool_val);
	case JSON_NUMBER:
		return json_value_create_number(value->data.number_val);
	case JSON_STRING:
		return json_value_create_string(value->data.string_val);
	case JSON_ARRAY:
		clone = json_value_create_array();
		if (!clone)
			return NULL;
		for (i = 0; i < value->data.array_val->count; i++) {
			if (json_array_append(clone->data.array_val,
					      &value->data.array_val->values[i]) < 0) {
				json_value_free(clone);
				return NULL;
			}
		}
		return clone;
	case JSON_OBJECT:
		clone = json_value_create_object();
		if (!clone)
			return NULL;
		for (i = 0; i < value->data.object_val->count; i++) {
			if (json_object_set(clone->data.object_val,
					    value->data.object_val->pairs[i].key,
					    &value->data.object_val->pairs[i].value) < 0) {
				json_value_free(clone);
				return NULL;
			}
		}
		return clone;
	}

	return NULL;
}

/**
 * json_value_free_contents - Free contents of a JSON value (not the value itself)
 * @value: value whose contents to free
 */
static void json_value_free_contents(struct json_value *value)
{
	size_t i;

	if (!value)
		return;

	switch (value->type) {
	case JSON_STRING:
		free(value->data.string_val);
		value->data.string_val = NULL;
		break;
	case JSON_ARRAY:
		if (value->data.array_val) {
			for (i = 0; i < value->data.array_val->count; i++)
				json_value_free_contents(&value->data.array_val->values[i]);
			free(value->data.array_val->values);
			free(value->data.array_val);
			value->data.array_val = NULL;
		}
		break;
	case JSON_OBJECT:
		if (value->data.object_val) {
			for (i = 0; i < value->data.object_val->count; i++) {
				free(value->data.object_val->pairs[i].key);
				json_value_free_contents(&value->data.object_val->pairs[i].value);
			}
			free(value->data.object_val->pairs);
			free(value->data.object_val);
			value->data.object_val = NULL;
		}
		break;
	default:
		break;
	}
}

/**
 * json_value_free - Free a JSON value and its contents
 * @value: value to free
 */
void json_value_free(struct json_value *value)
{
	if (!value)
		return;

	json_value_free_contents(value);
	free(value);
}

/**
 * diff_arrays - Create diff for two JSON arrays
 * @left: first array
 * @right: second array
 * @opts: diff options
 *
 * Return: diff object or NULL if arrays are equal
 */
static struct json_value *diff_arrays(const struct json_array *left,
				       const struct json_array *right,
				       const struct json_diff_options *opts)
{
	struct json_value *diff_obj = json_value_create_object();
	struct json_value *marker_val = json_value_create_string(ARRAY_MARKER_VALUE);
	char index_str[32];
	size_t i;
	bool has_changes = false;

	if (!diff_obj || !marker_val) {
		json_value_free(diff_obj);
		json_value_free(marker_val);
		return NULL;
	}

	/* Simple implementation: mark all changes */
	for (i = 0; i < left->count || i < right->count; i++) {
		if (i < left->count && i < right->count) {
			if (!json_value_equal(&left->values[i], &right->values[i],
					      opts->strict_equality)) {
				struct json_value *change_array = json_value_create_array();
				snprintf(index_str, sizeof(index_str), "%zu", i);
				
				json_array_append(change_array->data.array_val, &left->values[i]);
				json_array_append(change_array->data.array_val, &right->values[i]);
				json_object_set(diff_obj->data.object_val, index_str, change_array);
				json_value_free(change_array);
				has_changes = true;
			}
		} else if (i < left->count) {
			/* Deletion */
			struct json_value *del_array = json_value_create_array();
			struct json_value *zero = json_value_create_number(0);
			snprintf(index_str, sizeof(index_str), "_%zu", i);
			
			json_array_append(del_array->data.array_val, &left->values[i]);
			json_array_append(del_array->data.array_val, zero);
			json_array_append(del_array->data.array_val, zero);
			json_object_set(diff_obj->data.object_val, index_str, del_array);
			json_value_free(del_array);
			json_value_free(zero);
			has_changes = true;
		} else {
			/* Insertion */
			struct json_value *ins_array = json_value_create_array();
			snprintf(index_str, sizeof(index_str), "%zu", i);
			
			json_array_append(ins_array->data.array_val, &right->values[i]);
			json_object_set(diff_obj->data.object_val, index_str, ins_array);
			json_value_free(ins_array);
			has_changes = true;
		}
	}

	if (has_changes) {
		json_object_set(diff_obj->data.object_val, ARRAY_MARKER, marker_val);
		json_value_free(marker_val);
		return diff_obj;
	}

	json_value_free(diff_obj);
	json_value_free(marker_val);
	return NULL;
}

/**
 * json_diff - Create a diff between two JSON values
 * @left: first JSON value
 * @right: second JSON value
 * @opts: diff options (can be NULL for defaults)
 *
 * Return: diff object or NULL if values are equal
 */
struct json_value *json_diff(const struct json_value *left,
			     const struct json_value *right,
			     const struct json_diff_options *opts)
{
	struct json_diff_options default_opts = { .strict_equality = true };
	struct json_value *diff_obj;
	size_t i;
	bool has_changes = false;

	if (!opts)
		opts = &default_opts;

	if (json_value_equal(left, right, opts->strict_equality))
		return NULL;

	if (left->type != right->type || 
	    (left->type != JSON_OBJECT && left->type != JSON_ARRAY)) {
		/* Simple value change */
		struct json_value *change_array = json_value_create_array();
		json_array_append(change_array->data.array_val, left);
		json_array_append(change_array->data.array_val, right);
		return change_array;
	}

	if (left->type == JSON_ARRAY)
		return diff_arrays(left->data.array_val, right->data.array_val, opts);

	/* Object diff */
	diff_obj = json_value_create_object();
	if (!diff_obj)
		return NULL;

	/* Check all keys in left object */
	for (i = 0; i < left->data.object_val->count; i++) {
		const char *key = left->data.object_val->pairs[i].key;
		struct json_value *left_val = &left->data.object_val->pairs[i].value;
		struct json_value *right_val = json_object_get(right->data.object_val, key);

		if (!right_val) {
			/* Key deleted */
			struct json_value *del_array = json_value_create_array();
			struct json_value *zero = json_value_create_number(0);
			
			json_array_append(del_array->data.array_val, left_val);
			json_array_append(del_array->data.array_val, zero);
			json_array_append(del_array->data.array_val, zero);
			json_object_set(diff_obj->data.object_val, key, del_array);
			json_value_free(del_array);
			json_value_free(zero);
			has_changes = true;
		} else {
			/* Key exists in both, check for changes */
			struct json_value *sub_diff = json_diff(left_val, right_val, opts);
			if (sub_diff) {
				json_object_set(diff_obj->data.object_val, key, sub_diff);
				json_value_free(sub_diff);
				has_changes = true;
			}
		}
	}

	/* Check for new keys in right object */
	for (i = 0; i < right->data.object_val->count; i++) {
		const char *key = right->data.object_val->pairs[i].key;
		struct json_value *right_val = &right->data.object_val->pairs[i].value;
		struct json_value *left_val = json_object_get(left->data.object_val, key);

		if (!left_val) {
			/* Key added */
			struct json_value *add_array = json_value_create_array();
			json_array_append(add_array->data.array_val, right_val);
			json_object_set(diff_obj->data.object_val, key, add_array);
			json_value_free(add_array);
			has_changes = true;
		}
	}

	if (has_changes)
		return diff_obj;

	json_value_free(diff_obj);
	return NULL;
}

/**
 * json_patch - Apply a diff to a JSON value
 * @original: original JSON value
 * @diff: diff to apply
 *
 * Return: patched JSON value or NULL on failure
 */
struct json_value *json_patch(const struct json_value *original,
			      const struct json_value *diff)
{
	struct json_value *result;
	size_t i;

	if (!original || !diff)
		return NULL;

	if (diff->type == JSON_ARRAY && diff->data.array_val->count == 2) {
		/* Simple value replacement */
		return json_value_clone(&diff->data.array_val->values[1]);
	}

	if (original->type != JSON_OBJECT || diff->type != JSON_OBJECT)
		return json_value_clone(original);

	result = json_value_clone(original);
	if (!result)
		return NULL;

	/* Apply all changes from diff */
	for (i = 0; i < diff->data.object_val->count; i++) {
		const char *key = diff->data.object_val->pairs[i].key;
		struct json_value *diff_val = &diff->data.object_val->pairs[i].value;

		if (strcmp(key, ARRAY_MARKER) == 0)
			continue; /* Skip array marker */

		if (diff_val->type == JSON_ARRAY) {
			if (diff_val->data.array_val->count == 1) {
				/* Addition */
				json_object_set(result->data.object_val, key,
					       &diff_val->data.array_val->values[0]);
			} else if (diff_val->data.array_val->count == 3) {
				/* Deletion - remove key */
				/* This is simplified; full implementation would remove the key */
			} else if (diff_val->data.array_val->count == 2) {
				/* Replacement */
				json_object_set(result->data.object_val, key,
					       &diff_val->data.array_val->values[1]);
			}
		} else {
			/* Nested diff */
			struct json_value *orig_val = json_object_get(result->data.object_val, key);
			if (orig_val) {
				struct json_value *patched = json_patch(orig_val, diff_val);
				if (patched) {
					json_object_set(result->data.object_val, key, patched);
					json_value_free(patched);
				}
			}
		}
	}

	return result;
}
