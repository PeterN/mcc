#include <stdbool.h>
#include <string.h>
#include "client.h"
#include "commands.h"
#include "player.h"
#include "module.h"
#include "mcc.h"



static inline bool command_compare(struct command *a, struct command *b)
{
	return strcmp(a->command, b->command) == 0;
}

LIST(command, struct command, command_compare)
static struct command_list_t s_commands;

static int command_sort(const void *a, const void *b)
{
	return strcmp(((const struct command *)a)->command, ((const struct command *)b)->command);
}

void register_command(const char *command, enum rank_t rank, command_func func, const char *help)
{
	struct command cmd;
	cmd.command = command;
	cmd.rank = rank;
	cmd.func = func;
	cmd.help = help;
	command_list_add(&s_commands, cmd);

	/* Sort the command list so that we can display it in order easily... */
	qsort(s_commands.items, s_commands.used, sizeof (struct command), &command_sort);
}

void deregister_command(const char *command)
{
	struct command cmd;
	cmd.command = command;
	command_list_del_item(&s_commands, cmd);

	if (!g_server.exit)
	{
		qsort(s_commands.items, s_commands.used, sizeof (struct command), &command_sort);
	}
}

#define CMD(x) static bool cmd_ ## x (struct client_t *c, int params, const char **param)

static const char help_commands[] =
"/commands\n"
"List all commands available to you.";

CMD(commands)
{
	if (params != 1) return true;

	char buf[64];
	char *bufp = buf;
	char *endp = buf + sizeof buf;

	/* Count number of commands to be output */
	int count = -1;
	unsigned i;
	for (i = 0; i < s_commands.used; i++)
	{
		struct command *comp = &s_commands.items[i];
		if (c->player->rank < comp->rank) continue;
		count++;
	}

	bufp += snprintf(bufp, endp - bufp, TAG_YELLOW "Commands:" TAG_WHITE " ");
	for (i = 0; i < s_commands.used; i++)
	{
		struct command *comp = &s_commands.items[i];
		if (c->player->rank < comp->rank) continue;

		bool last = i == count;

		if (strlen(comp->command) + (last ? 0 : 1) >= endp - bufp)
		{
			client_notify(c, buf);
			bufp = buf;
		}

		bufp += snprintf(bufp, endp - bufp, "%s%s", comp->command, last ? "" : ", ");
	}
	client_notify(c, buf);

	return false;
}


static const char help_exit[] =
"/exit\n"
"Exit and shut down the server.";

CMD(exit)
{
	if (params != 1) return true;

	g_server.exit = true;
	return false;
}

static const char help_help[] =
"/help <command>\n"
"Display help about a command. See /commands.";

CMD(help)
{
	if (params != 2) return true;

	unsigned i;
	for (i = 0; i < s_commands.used; i++)
	{
		struct command *comp = &s_commands.items[i];
		if (strcasecmp(comp->command, param[1]) == 0)
		{
			client_notify(c, comp->help);
			return false;
		}
	}

	client_notify(c, "Command not found");
	return false;
}

static const char help_module_load[] =
"/module_load <name>\n"
"Load a module.";

CMD(module_load)
{
	if (params != 2) return true;

	module_load(param[1]);
	return false;
}

static const char help_module_unload[] =
"/module_unload <name>\n"
"Unload a module.";

CMD(module_unload)
{
	if (params != 2) return true;

	struct module_t *m = module_get_by_name(param[1]);
	if (m == 0)
	{
		client_notify(c, "Module not loaded");
		return false;
	}

	module_unload(m);
	return false;
}

static const char help_modules[] =
"/modules\n"
"List loaded modules.";

CMD(modules)
{
	if (params != 1) return true;

	char buf[64];
	char *bufp = buf;
	char *endp = buf + sizeof buf;

	unsigned count = s_modules.used - 1;
	unsigned i;

	bufp += snprintf(bufp, endp - bufp, TAG_YELLOW "Modules:" TAG_WHITE " ");
	for (i = 0; i < s_modules.used; i++)
	{
		const char *name = s_modules.items[i]->name;
		bool last = i == count;

		if (strlen(name) + (last ? 0 : 1) >= endp - bufp)
		{
			client_notify(c, buf);
			bufp = buf;
		}

		bufp += snprintf(bufp, endp - bufp, "%s%s", name, last ? "" : ", ");
	}

	client_notify(c, buf);

	return false;
}


static const struct command s_builtin_commands[] = {
	{ "commands", RANK_BANNED, &cmd_commands, help_commands },
	{ "exit", RANK_ADMIN, &cmd_exit, help_exit },
	{ "help", RANK_GUEST, &cmd_help, help_help },
	{ "module_load", RANK_ADMIN, &cmd_module_load, help_module_load },
	{ "module_unload", RANK_ADMIN, &cmd_module_unload, help_module_unload },
	{ "modules", RANK_ADMIN, &cmd_modules, help_modules },
	{ NULL, -1, NULL, NULL },
};

void commands_init()
{
	const struct command *comp = s_builtin_commands;
	for (; comp->command != NULL; comp++)
	{
		register_command(comp->command, comp->rank, comp->func, comp->help);
	}
}

void commands_deinit()
{
	const struct command *comp = s_builtin_commands;
	for (; comp->command != NULL; comp++)
	{
		deregister_command(comp->command);
	}
}

bool command_process(struct client_t *client, int params, const char **param)
{
	unsigned i;
	for (i = 0; i < s_commands.used; i++)
	{
		struct command *comp = &s_commands.items[i];
		if (client->player->rank >= comp->rank && strcasecmp(param[0], comp->command) == 0)
		{
			if (comp->func(client, params, param))
			{
				client_notify(client, comp->help);
			}
			return true;
		}
	}

	return false;
}


