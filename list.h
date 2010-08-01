#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include "mcc.h"

#define LIST(X, T, compare_func) \
struct X ## _list_t \
{ \
	size_t used; \
	size_t size; \
	T *items; \
}; \
\
static inline void X ## _list_init(struct X ## _list_t *list) \
{ \
	list->used = 0; \
	list->size = 0; \
	list->items = NULL; \
} \
\
static inline void X ## _list_free(struct X ## _list_t *list) \
{ \
	free(list->items); \
} \
\
static inline void X ## _list_add(struct X ## _list_t *list, T item) \
{ \
	if (list->used >= list->size) \
	{ \
		list->size += 64U; \
		T *new_items = realloc(list->items, sizeof *list->items * list->size); \
		if (new_items == NULL) \
		{ \
			LOG("Reallocating X list to %u items (%u bytes) failed\n", list->size, sizeof *list->items * list->size); \
			return; \
		} \
		list->items = new_items; \
	} \
	list->items[list->used++] = item; \
} \
\
static inline void X ## _list_del_item(struct X ## _list_t *list, T item) \
{ \
	size_t i; \
	for (i = 0; i < list->used; i++) \
	{ \
		if (compare_func(&list->items[i], &item)) { \
			list->items[i] = list->items[--list->used]; \
			return; \
		} \
	} \
} \
\
static inline void X ## _list_del_index(struct X ## _list_t *list, size_t index) \
{ \
	list->items[index] = list->items[--list->used]; \
}

#endif /* LIST_H */
