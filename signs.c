#include <stdio.h>
#include <string.h>
#include "bitstuff.h"
#include "block.h"
#include "colour.h"
#include "client.h"
#include "level.h"
#include "player.h"
#include "position.h"
#include "mcc.h"

#define LINES_PER_MESSAGE 4

struct message_t
{
	char name[16];
	char line[LINES_PER_MESSAGE][64];
	int radius;
	struct position_t pos;
};

struct message_data_t
{
	int16_t messages;
	struct message_t *edit;
	struct message_t message[];
};

static struct message_t *message_get_by_name(const char *name, struct message_data_t *arg, struct level_hook_data_t *ld, bool create)
{
	int i;

	/* Find existing message */
	for (i = 0; i < arg->messages; i++)
	{
		if (strncasecmp(arg->message[i].name, name, sizeof arg->message[i].name - 1) == 0) return &arg->message[i];
	}

	if (!create) return NULL;

	/* Find empty slot for new message */
	for (i = 0; i < arg->messages; i++)
	{
		if (*arg->message[i].name == '\0')
		{
			memset(&arg->message[i], 0, sizeof arg->message[i]);
			snprintf(arg->message[i].name, sizeof arg->message[i].name, "%s", name);
			return &arg->message[i];
		}
	}

	/* Create new slot */
	ld->size = sizeof (struct message_data_t) + sizeof (struct message_t) * (arg->messages + 1);
	arg = realloc(ld->data, ld->size);
	if (arg == NULL) return NULL;
	ld->data = arg;

	memset(&arg->message[i], 0, sizeof arg->message[i]);
	snprintf(arg->message[i].name, sizeof arg->message[i].name, "%s", name);
	arg->messages++;
	return &arg->message[i];
}

static void message_handle_chat(struct level_t *l, struct client_t *c, char *data, struct message_data_t *arg, struct level_hook_data_t *ld)
{
	if (!level_user_can_build(l, c->player)) return;

	if (strncasecmp(data, "msg edit ", 9) == 0)
	{
		struct message_t *s = message_get_by_name(data + 9, arg, ld, true);
		if (s == NULL) return;
		arg = ld->data;
		arg->edit = s;
		char buf[128];
		snprintf(buf, sizeof buf, TAG_YELLOW "Editing message %s", s->name);
		client_notify(c, buf);
	}
	else if (strncasecmp(data, "msg delete ", 11) == 0)
	{
		struct message_t *s = message_get_by_name(data + 11, arg, ld, false);
		if (s == NULL) return;
		memset(s, 0, sizeof *s);
		client_notify(c, TAG_YELLOW "message deleted");
		if (arg->edit == s) arg->edit = NULL;
		l->changed = true;
	}
	else if (strncasecmp(data, "msg rename ", 11) == 0)
	{
		struct message_t *s = arg->edit;
		if (s == NULL) return;
		snprintf(s->name, sizeof s->name, data + 11);
		client_notify(c, TAG_YELLOW "message renamed");
		l->changed = true;
	}
	else if (strcasecmp(data, "msg place") == 0)
	{
		struct message_t *s = arg->edit;
		if (s == NULL) return;
		s->pos = c->player->pos;
		client_notify(c, TAG_YELLOW "message position set");
		l->changed = true;
	}
	else if (strncasecmp(data, "msg radius ", 11) == 0)
	{
		struct message_t *s = arg->edit;
		if (s == NULL) return;
		s->radius = strtol(data + 11, NULL, 10);
		char buf[64];
		snprintf(buf, sizeof buf, TAG_YELLOW "message radius set to %d", s->radius);
		client_notify(c, buf);
		l->changed = true;
	}
	else if (strncasecmp(data, "msg l1 ", 7) == 0)
	{
		struct message_t *s = arg->edit;
		if (s == NULL) return;
		snprintf(s->line[0], sizeof s->line[0], "%s", data + 7);
		client_notify(c, TAG_YELLOW "message line 1 set");
		l->changed = true;
	}
	else if (strncasecmp(data, "msg l2 ", 7) == 0)
	{
		struct message_t *s = arg->edit;
		if (s == NULL) return;
		snprintf(s->line[1], sizeof s->line[1], "%s", data + 7);
		client_notify(c, TAG_YELLOW "message line 2 set");
		l->changed = true;
	}
	else if (strncasecmp(data, "msg l3 ", 7) == 0)
	{
		struct message_t *s = arg->edit;
		if (s == NULL) return;
		snprintf(s->line[2], sizeof s->line[2], "%s", data + 7);
		client_notify(c, TAG_YELLOW "message line 3 set");
		l->changed = true;
	}
	else if (strncasecmp(data, "msg l4 ", 7) == 0)
	{
		struct message_t *s = arg->edit;
		if (s == NULL) return;
		snprintf(s->line[3], sizeof s->line[3], "%s", data + 7);
		client_notify(c, TAG_YELLOW "message line 4 set");
		l->changed = true;
	}
	else if (strcasecmp(data, "msg list") == 0)
	{
		unsigned i;
		for (i = 0; i < arg->messages; i++)
		{
			struct message_t *s = &arg->message[i];
			if (*s->name != '\0')
			{
				char buf[128];
				snprintf(buf, sizeof buf, TAG_YELLOW "message %s at %dx%dx%d", s->name, s->pos.x, s->pos.y, s->pos.z);
				client_notify(c, buf);
			}
		}
	}
}

static void message_handle_move(struct level_t *l, struct client_t *c, int index, struct message_data_t *arg)
{
	/* Changing levels, don't handle messages */
	if (c->player->level != c->player->new_level) return;

	int i, j;
	for (i = 0; i < arg->messages; i++)
	{
		struct message_t *s = &arg->message[i];
		if (position_match(&c->player->pos, &s->pos, s->radius))
		{
			if (HasBit(c->player->flags, 6)) return;

			for (j = 0; j < LINES_PER_MESSAGE; j++)
			{
				if (strlen(s->line[j]) > 0)
				{
					client_notify(c, s->line[j]);
				}
			}

			SetBit(c->player->flags, 6);

			return;
		}
	}

	if (HasBit(c->player->flags, 6))
	{
		ClrBit(c->player->flags, 6);
	}
}

static void message_level_hook(int event, struct level_t *l, struct client_t *c, void *data, struct level_hook_data_t *arg)
{
	switch (event)
	{
		case EVENT_CHAT: message_handle_chat(l, c, data, arg->data, arg); break;
		case EVENT_MOVE: message_handle_move(l, c, *(int *)data, arg->data); break;
		case EVENT_INIT:
		{
			if (arg->size == 0)
			{
				LOG("Allocating new message data on %s\n", l->name);
			}
			else
			{
				struct message_data_t *sd = arg->data;
				if (arg->size == sizeof (struct message_data_t) + sizeof (struct message_t) * sd->messages)
				{
					LOG("Found data for %d messages on %s\n", sd->messages, l->name);

					/* Ensure message edit isn't set after loading */
					sd->edit = NULL;
					break;
				}

				LOG("Found invalid message data on %s, erasing\n", l->name);
				free(arg->data);
			}

			arg->size = sizeof (struct message_data_t);
			arg->data = calloc(1, arg->size);
			break;
		}
	}
}

void module_init(void **data)
{
	register_level_hook_func("messages", &message_level_hook);
}

void module_deinit(void *data)
{
	deregister_level_hook_func("messages");
}
