#ifndef MODULE_H
#define MODULE_H

#include "list.h"

struct module_t
{
	char name[64];
	void *data;

	void *handle;
	void (*init_func)(void **data);
	void (*deinit_func)(void *data);
};

static inline bool module_t_compare(struct module_t **a, struct module_t **b)
{
	return *a == *b;
}

LIST(module, struct module_t *, module_t_compare)

extern struct module_list_t s_modules;

void module_load(const char *name);
void module_unload(struct module_t *m);
struct module_t *module_get_by_name(const char *name);
void module_init(struct module_t *m);
void module_deinit(struct module_t *m);

void modules_init();
void modules_deinit();

#endif /* MODULE_H */
