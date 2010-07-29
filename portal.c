#include <stdio.h>
#include <string.h>
#include "bitstuff.h"
#include "block.h"
#include "colour.h"
#include "client.h"
#include "level.h"
#include "player.h"
#include "position.h"

#define MAX_PORTALS 8

struct portal_t
{
	char name[16];
	char target[16];
	char target_level[32];
	struct position_t pos;
};

struct portal_data_t
{
	int16_t portals;
	struct portal_t portal[MAX_PORTALS];
	struct portal_t *edit;
};

static struct portal_t *portal_get_by_name(char *name, struct portal_data_t *arg)
{
	int i;
	for (i = 0; i < arg->portals; i++)
	{
		if (strcasecmp(arg->portal[i].name, name) == 0) return &arg->portal[i];
	}

	if (i < MAX_PORTALS)
	{
		strncpy(arg->portal[i].name, name, sizeof arg->portal[i].name);
		arg->portals++;
		return &arg->portal[i];
	}

	return NULL;
}

static void portal_handle_chat(struct level_t *l, struct client_t *c, char *data, struct portal_data_t *arg)
{
	if (strncasecmp(data, "portal edit ", 12) == 0)
	{
		struct portal_t *p = portal_get_by_name(data + 12, arg);
		if (p == NULL) return;
		arg->edit = p;
		char buf[128];
		snprintf(buf, sizeof buf, TAG_YELLOW "Editing portal %s", data + 12);
		client_notify(c, buf);
	}
	else if (strncasecmp(data, "portal place", 12) == 0)
	{
		struct portal_t *p = arg->edit;
		if (p == NULL) return;
		p->pos = c->player->pos;
		client_notify(c, "Portal position set");
	}
	else if (strncasecmp(data, "portal target ", 14) == 0)
	{
		struct portal_t *p = arg->edit;
		if (p == NULL) return;
		strncpy(p->target, data + 14, sizeof p->target);
		client_notify(c, "Portal target set");
	}
	else if (strncasecmp(data, "portal target-level ", 20) == 0)
	{
		struct portal_t *p = arg->edit;
		if (p == NULL) return;
		strncpy(p->target, data + 20, sizeof p->target);
		client_notify(c, "Portal target-level set");
	}
}

static void portal_handle_move(struct level_t *l, struct client_t *c, int index, struct portal_data_t *arg)
{
	int i;
	for (i = 0; i < arg->portals; i++)
	{
		struct portal_t *p = &arg->portal[i];
		if (position_match(&c->player->pos, &p->pos, 32))
		{
			if (HasBit(c->player->flags, 7)) return;

			char buf[128];
			snprintf(buf, sizeof buf, "Reached portal %s", p->name);
			client_notify(c, buf);

			SetBit(c->player->flags, 7);

			/* Portal is exit only */
			if (*p->target == '\0') return;
			if (*p->target_level == '\0')
			{
				struct portal_t *p2 = portal_get_by_name(p->target, arg);
				if (p2 == NULL)
				{
					snprintf(buf, sizeof buf, "Portal target %s not found", p->target);
					client_notify(c, buf);
				}
				else if (p2->pos.x == 0 && p2->pos.y == 0 && p2->pos.z == 0)
				{
					snprintf(buf, sizeof buf, "Portal target %s has no position", p->target);
					client_notify(c, buf);
				}
				else
				{
					snprintf(buf, sizeof buf, "Teleporting to %s", p->target);
					client_notify(c, buf);
					c->player->pos = p2->pos;
					client_add_packet(c, packet_send_teleport_player(0xFF, &c->player->pos));
				}
			}
			else
			{
				/* Switch level */
			}
			return;
		}
	}

	if (HasBit(c->player->flags, 7))
	{
		client_notify(c, "Moved out of portal");
		c->player->flags ^= 1 << 7;
	}
}

static void portal_level_hook(int event, struct level_t *l, struct client_t *c, void *data, struct level_hook_data_t *arg)
{
	switch (event)
	{
		case EVENT_CHAT: portal_handle_chat(l, c, data, arg->data); break;
		case EVENT_MOVE: portal_handle_move(l, c, *(int *)data, arg->data); break;
//		case EVENT_LOAD: portal->edit = NULL; break;
		case EVENT_INIT:
		{
			if (arg->size != sizeof (struct portal_data_t))
			{
				arg->size = sizeof (struct portal_data_t);
				arg->data = calloc(1, arg->size);
			}
			break;
		}
	}
}

void module_init(void **data)
{
	register_level_hook_func("portal", &portal_level_hook);
}

void module_deinit(void *data)
{
	deregister_level_hook_func("portal");
}
