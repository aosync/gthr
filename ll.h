#include <stdlib.h>

#ifndef T
#error ll.h: T not defined
#endif

#define TPL_(type, name) type ## name
#define TPL(type, name) TPL_(type, name)

typedef struct TPL(T, ll__) {
	T					val;
	struct TPL(T, ll__)	*next;
	struct TPL(T, ll__)	*prev;
} TPL(T, ll_);

typedef struct {
	TPL(T, ll_) *head;
	TPL(T, ll_) *back;
} TPL(T, ll);

static void
TPL(T, ll_init)(TPL(T, ll) *ll)
{
	ll->head = NULL;
	ll->back = NULL;
}

static T
TPL(T, ll_insert)(TPL(T, ll) *ll, T what)
{
	TPL(T, ll_) *n = malloc(sizeof(TPL(T, ll_)));
	n->val = what;
	ll->head->prev = n;
	n->next = ll->head;
	n->prev = NULL;
	if (ll->back == NULL) ll->back = n;
	ll->head = n;
	return what;
}

static T
TPL(T, ll_insert_back)(TPL(T, ll) *ll, T what)
{
	TPL(T, ll_) *n = malloc(sizeof(TPL(T, ll_)));
	n->val = what;
	n->next = NULL;
	n->prev = ll->back;
	if (ll->back) ll->back->next = n;
	ll->back = n;
	if (ll->head == NULL) ll->head = n;
	return what;
}

static T
TPL(T, ll_insert_after)(TPL(T, ll) *ll, TPL(T, ll_) *after, T what)
{
	TPL(T, ll_) *n = malloc(sizeof(TPL(T, ll_)));
	n->val = what;
	n->next = after->next;
	after->next = n;
	return what;
}

static TPL(T, ll_) *
TPL(T, ll_lookup)(TPL(T, ll) *ll, T what)
{
	TPL(T, ll_) *next = ll->head;
	while (next) {
		if (TPL(T, ll_equals(what, next->val))) return next;
		next = next->next;
	}
	return NULL;
}

static void
TPL(T, ll_free)(TPL(T, ll) *ll)
{
	TPL(T, ll_) *tmp;
	TPL(T, ll_) *next = ll->head;
	while (next) {
		tmp = next->next;
		free(next);
		next = tmp;
	}
	ll->head = NULL;
	ll->back = NULL;
}

#undef T
