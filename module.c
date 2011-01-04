#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include "mcc.h"
#include "module.h"

struct module_list_t s_modules;

void module_load(const char *name)
{
	if (module_get_by_name(name) != NULL)
	{
		LOG("Module %s already registered\n", name);
		return;
	}

	struct module_t *m = malloc(sizeof *m);

	strncpy(m->name, name, sizeof m->name);
	m->handle = dlopen(name, RTLD_LAZY);
	if (m->handle == NULL)
	{
		LOG("dlopen: %s\n", dlerror());
		free(m);
		return;
	}

	m->init_func = dlsym(m->handle, "module_init");
	m->deinit_func = dlsym(m->handle, "module_deinit");

	module_list_add(&s_modules, m);

	module_init(m);
}

void module_unload(struct module_t *m)
{
	module_deinit(m);

	dlclose(m->handle);
	free(m);

	module_list_del_item(&s_modules, m);
}

struct module_t *module_get_by_name(const char *name)
{
	unsigned i;
	for (i = 0; i < s_modules.used; i++)
	{
		struct module_t *m = s_modules.items[i];
		if (strcasecmp(m->name, name) == 0) return m;
	}

	return NULL;
}

void module_init(struct module_t *m)
{
	LOG("Initializing module %s\n", m->name);
	m->init_func(&m->data);
}

void module_deinit(struct module_t *m)
{
	if (m->deinit_func != NULL)
	{
		m->deinit_func(m->data);
	}
}

void modules_init(void)
{
	FILE *f = fopen("modules.txt", "r");
	if (f == NULL)
	{
		LOG("No module list to load\n");
		return;
	}

	while (!feof(f))
	{
		char buf[1024];
		if (fgets(buf, sizeof buf, f) > 0)
		{

			if (buf[0] == '#') continue;

			/* Remove newline characters */
			char *eol = strchr(buf, '\n');
			if (eol != NULL) *eol = '\0';

			module_load(buf);
		}
	}

	fclose(f);
}

void modules_deinit(void)
{

	while (s_modules.used > 0)
	{
		module_unload(s_modules.items[0]);
	}

	module_list_free(&s_modules);
}
