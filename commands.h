#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdbool.h>

struct client_t;

bool command_process(struct client_t *client, int params, const char **param);

#endif /* COMMANDS_H */
