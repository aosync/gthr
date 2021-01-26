#include <stdlib.h>
#include <string.h>

#ifndef T
#error vec.h: T not defined
#endif

#define TPL_(type, name) type ## name
#define TPL(type, name) TPL_(type, name)

typedef struct {
	size_t 	len;
	size_t 	blocks;
	T		*arr;
} TPL(T, v);

static void
TPL(T, v_init)(TPL(T, v) *vec)
{
	vec->len = 0;
	vec->blocks = 0;
	vec->arr = NULL;
}

static T
TPL(T, v_push)(TPL(T, v) *vec, T what)
{
	if (vec->len >= vec->blocks) {
		vec->blocks = vec->blocks ? vec->blocks * 2 : 1;
		vec->arr = realloc(vec->arr, vec->blocks * sizeof(T));
	}
	vec->arr[vec->len++] = what;
	return what;
}

static T
TPL(T, v_pop)(TPL(T, v) *vec)
{
	T what;
	if (vec->len == 0) return what;
	what = vec->arr[--vec->len];
	if (vec->len <= vec->blocks/2) {
		vec->blocks /= 2;
		vec->arr = realloc(vec->arr, vec->blocks * sizeof(T));
	}
	return what;
}

static T
TPL(T, v_insert)(TPL(T, v) *vec, size_t where, T what)
{
	size_t amount = (vec->len - where) * sizeof(T);
	vec->len++;
	if (vec->len >= vec->blocks) {
		// vec->blocks *= 2;
		vec->blocks = vec->blocks ? vec->blocks * 2 : 1;
		vec->arr = realloc(vec->arr, vec->blocks * sizeof(T));
	}
	memmove(vec->arr + where + 1, vec->arr + where, amount);
	vec->arr[where] = what;
	return what;
}

static T
TPL(T, v_erase)(TPL(T, v) *vec, size_t where)
{
	size_t amount = (vec->len - where - 1) * sizeof(T);
	T what = vec->arr[where];
	vec->len--;
	memmove(vec->arr + where, vec->arr + where + 1, amount);
	if (vec->len <= vec->blocks/2) {
		vec->blocks /= 2;
		vec->arr = realloc(vec->arr, vec->blocks * sizeof(T));
	}
	return what;
}

static void
TPL(T, v_free)(TPL(T, v) *vec)
{
	free(vec->arr);
	vec->arr = NULL;
	vec->blocks = 0;
	vec->len = 0;
}

#undef T