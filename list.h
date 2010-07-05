#ifndef LIST_H
#define LIST_H

#include <stdlib.h>

#define LIST(X, compare_func) \
struct X ## _list_t \
{ \
	size_t used; \
	size_t size; \
	struct X *items; \
}; \
\
static inline void X ## _list_init(struct X ## _list_t *list) \
{ \
	list->used = 0; \
	list->size = 0; \
	list->items = NULL; \
} \
\
static inline void X ## _list_add(struct X ## _list_t *list, struct X item) \
{ \
	if (list->used >= list->size) \
	{ \
		list->size += sizeof *list->items * 64U; \
		list->items = realloc(list->items, list->size); \
	} \
	list->items[list->used++] = item; \
} \
\
static inline void X ## _list_del(struct X ## _list_t *list, struct X item) \
{ \
	size_t i; \
	for (i = 0; i < list->used; i++) \
	{ \
		if (compare_func(&list->items[i], &item)) { \
			list->items[i] = list->items[--list->used]; \
			return; \
		} \
	} \
}

#endif /* LIST_H */
