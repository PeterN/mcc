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

#define LINES_PER_SIGN 4

struct sign_t
{
	char name[16];
	char message[LINES_PER_SIGN][64];
	struct position_t pos;
};

struct sign_data_t
{
	int16_t signs;
	struct sign_t *edit;
	struct sign_t sign[];
};

static struct sign_t *sign_get_by_name(const char *name, struct sign_data_t *arg, struct level_hook_data_t *ld, bool create)
{
	int i;

	/* Find existing sign */
	for (i = 0; i < arg->signs; i++)
	{
		if (strncasecmp(arg->sign[i].name, name, sizeof arg->sign[i].name - 1) == 0) return &arg->sign[i];
	}

	if (!create) return NULL;

	/* Find empty slot for new sign */
	for (i = 0; i < arg->signs; i++)
	{
		if (*arg->sign[i].name == '\0')
		{
			memset(&arg->sign[i], 0, sizeof arg->sign[i]);
			snprintf(arg->sign[i].name, sizeof arg->sign[i].name, "%s", name);
			return &arg->sign[i];
		}
	}

	/* Create new slot */
	ld->size = sizeof (struct sign_data_t) + sizeof (struct sign_t) * (arg->signs + 1);
	arg = realloc(ld->data, ld->size);
	if (arg == NULL) return NULL;
	ld->data = arg;

	memset(&arg->sign[i], 0, sizeof arg->sign[i]);
	snprintf(arg->sign[i].name, sizeof arg->sign[i].name, "%s", name);
	arg->signs++;
	return &arg->sign[i];
}

static void sign_handle_chat(struct level_t *l, struct client_t *c, char *data, struct sign_data_t *arg, struct level_hook_data_t *ld)
{
	if (!level_user_can_build(l, c->player)) return;

	if (strncasecmp(data, "sign edit ", 10) == 0)
	{
		struct sign_t *s = sign_get_by_name(data + 10, arg, ld, true);
		if (s == NULL) return;
		arg = ld->data;
		arg->edit = s;
		char buf[128];
		snprintf(buf, sizeof buf, TAG_YELLOW "Editing sign %s", s->name);
		client_notify(c, buf);
	}
	else if (strncasecmp(data, "sign delete ", 12) == 0)
	{
		struct sign_t *s = sign_get_by_name(data + 12, arg, ld, false);
		if (s == NULL) return;
		memset(s, 0, sizeof *s);
		client_notify(c, TAG_YELLOW "Sign deleted");
		if (arg->edit == s) arg->edit = NULL;
		l->changed = true;
	}
	else if (strncasecmp(data, "sign rename ", 12) == 0)
	{
		struct sign_t *s = arg->edit;
		if (s == NULL) return;
		snprintf(s->name, sizeof s->name, data + 12);
		client_notify(c, TAG_YELLOW "Sign renamed");
		l->changed = true;
	}
	else if (strcasecmp(data, "sign place") == 0)
	{
		struct sign_t *s = arg->edit;
		if (s == NULL) return;
		s->pos = c->player->pos;
		client_notify(c, TAG_YELLOW "Sign position set");
		l->changed = true;
	}
	else if (strcasecmp(data, "sign list") == 0)
	{
		unsigned i;
		for (i = 0; i < arg->signs; i++)
		{
			struct sign_t *s = &arg->sign[i];
			if (*s->name != '\0')
			{
				char buf[128];
				snprintf(buf, sizeof buf, TAG_YELLOW "Sign %s at %dx%dx%d", s->name, s->pos.x, s->pos.y, s->pos.z);
				client_notify(c, buf);
			}
		}
	}
}

static void sign_handle_move(struct level_t *l, struct client_t *c, int index, struct sign_data_t *arg)
{
	/* Changing levels, don't handle signs */
	if (c->player->level != c->player->new_level) return;

	int i, j;
	for (i = 0; i < arg->signs; i++)
	{
		struct sign_t *s = &arg->sign[i];
		if (position_match(&c->player->pos, &s->pos, 32))
		{
			if (HasBit(c->player->flags, 6)) return;

			for (j = 0; j < LINES_PER_SIGN; j++)
			{
				client_notify(c, s->message[j]);
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

static void sign_level_hook(int event, struct level_t *l, struct client_t *c, void *data, struct level_hook_data_t *arg)
{
	switch (event)
	{
		case EVENT_CHAT: sign_handle_chat(l, c, data, arg->data, arg); break;
		case EVENT_MOVE: sign_handle_move(l, c, *(int *)data, arg->data); break;
		case EVENT_INIT:
		{
			if (arg->size == 0)
			{
				LOG("Allocating new sign data on %s\n", l->name);
			}
			else
			{
				struct sign_data_t *sd = arg->data;
				if (arg->size == sizeof (struct sign_data_t) + sizeof (struct sign_t) * sd->signs)
				{
					LOG("Found data for %d signs on %s\n", sd->signs, l->name);

					/* Ensure sign edit isn't set after loading */
					sd->edit = NULL;
					break;
				}

				LOG("Found invalid sign data on %s, erasing\n", l->name);
				free(arg->data);
			}

			arg->size = sizeof (struct sign_data_t);
			arg->data = calloc(1, arg->size);
			break;
		}
	}
}

void module_init(void **data)
{
	register_level_hook_func("sign", &sign_level_hook);
}

void module_deinit(void *data)
{
	deregister_level_hook_func("sign");
}
