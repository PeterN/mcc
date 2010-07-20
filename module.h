#ifndef MODULE_H
#define MODULE_H

struct module_t
{
	char name[64];
	void *data;

	void *handle;
	void (*init_func)(void **data);
	void (*deinit_func)(void *data);
};

void module_load(const char *name);
void module_unload(struct module_t *m);
void module_init(struct module_t *m);
void module_deinit(struct module_t *m);

#endif /* MODULE_H */
