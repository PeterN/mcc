#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdbool.h>
#include "rank.h"

struct client_t;

typedef bool(*command_func)(struct client_t *c, int params, const char **param);

struct command
{
	const char *command;
	enum rank_t rank;
	command_func func;
	const char *help;
};

void register_command(const char *command, enum rank_t rank, command_func func, const char *help);
void deregister_command(const char *command);

void commands_init();
void commands_deinit();

bool command_process(struct client_t *client, int params, const char **param);
void notify_file(struct client_t *c, const char *filename);

#endif /* COMMANDS_H */
