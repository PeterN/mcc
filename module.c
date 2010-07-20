#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include "mcc.h"
#include "module.h"

void module_load(const char *name)
{
	struct module_t m;

	strncpy(m.name, name, sizeof m.name);
	m.handle = dlopen(name, RTLD_LAZY);
	if (m.handle == NULL)
	{
		LOG("dlopen: %s", dlerror());
		return;
	}

	m.init_func = dlsym(m.handle, "module_init");
	m.deinit_func = dlsym(m.handle, "module_deinit");

	module_init(&m);
}

void module_unload(struct module_t *m)
{
	dlclose(m->handle);
}

void module_init(struct module_t *m)
{
	LOG("Initializing module %s\n", m->name);
	m->init_func(&m->data);
}

void module_deinit(struct module_t *m)
{
	m->deinit_func(m->data);
}
