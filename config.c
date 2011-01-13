#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mcc.h"

#define MAXCONFIGLEN 64

struct configitem
{
	char setting[MAXCONFIGLEN];
	char value[MAXCONFIGLEN];
	struct configitem *next;
};

static struct
{
	const char *filename;
	struct configitem *head;
} s_config;

static void config_free(void)
{
	struct configitem *prev;
	struct configitem *curr = s_config.head;

	while (curr != NULL)
	{
		prev = curr;
		curr = curr->next;
		free(prev);
	}
}

void config_set_string(const char *key, const char *value)
{
	struct configitem *item;
	struct configitem **itemp = &s_config.head;

	while (*itemp != NULL)
	{
		if (strcasecmp((*itemp)->setting, key) == 0)
		{
			item = *itemp;
			if (value == NULL)
			{
				*itemp = item->next;
				free(item);
			}
			else
			{
				strncpy(item->value, value, sizeof item->value);
			}
			return;
		}
		itemp = &(*itemp)->next;
	}

	if (value == NULL) return;

	*itemp = malloc(sizeof *item);
	item = *itemp;

	strncpy(item->setting, key, sizeof item->setting);
	strncpy(item->value, value, sizeof item->value);
	item->next = NULL;
}

static void config_rehash(void)
{
	FILE *f = fopen(s_config.filename, "r");
	if (f == NULL) return;

	config_free();

	while (!feof(f))
	{
		char buf[256];
		if (fgets(buf, sizeof buf, f) <= 0) break;

		/* Ignore comments */
		if (*buf == '#') continue;

		char *n = strchr(buf, '=');
		if (n == NULL) continue;
		*n++ = '\0';

		char *eol = strchr(n, '\n');
		if (eol != NULL) *eol = '\0';

		config_set_string(buf, n);
	}

	fclose(f);
}

static void config_write(void)
{
	FILE *f = fopen(s_config.filename, "w");
	if (f == NULL) return;

	struct configitem *curr;
	for (curr = s_config.head; curr != NULL; curr = curr->next)
	{
		fprintf(f, "%s=%s\n", curr->setting, curr->value);
	}

	fclose(f);
}

void config_init(const char *filename)
{
	s_config.filename = filename;
	s_config.head = NULL;

	config_rehash();
}

void config_deinit(void)
{
	config_write();
}

void config_set_int(const char *setting, int value)
{
	char c[MAXCONFIGLEN];
	snprintf(c, sizeof c, "%d", value);
	config_set_string(setting, c);
}

bool config_get_string(const char *setting, char **value)
{
	struct configitem *curr;
	for (curr = s_config.head; curr != NULL; curr = curr->next)
	{
		if (strcasecmp(curr->setting, setting) == 0)
		{
			*value = curr->value;
			return true;
		}
	}

	*value = NULL;
	return false;
}

bool config_get_int(const char *setting, int *value)
{
	char *c, *endptr;
	int v;

	if (!config_get_string(setting, &c)) return false;

	v = strtol(c, &endptr, 10);
	if (*endptr != '\0') return false;

	*value = v;
	return true;
}

