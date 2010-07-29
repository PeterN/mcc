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

static struct portal_t *portal_get_by_name(const char *name, struct portal_data_t *arg, bool create)
{
	int i;
	for (i = 0; i < arg->portals; i++)
	{
		if (strcasecmp(arg->portal[i].name, name) == 0) return &arg->portal[i];
	}

	if (create && i < MAX_PORTALS)
	{
		strncpy(arg->portal[i].name, name, sizeof arg->portal[i].name);
		arg->portals++;
		return &arg->portal[i];
	}

	return NULL;
}

static void portal_handle_chat(struct level_t *l, struct client_t *c, char *data, struct portal_data_t *arg)
{
	if (!level_user_can_build(l, c->player)) return;

	if (strncasecmp(data, "portal edit ", 12) == 0)
	{
		struct portal_t *p = portal_get_by_name(data + 12, arg, true);
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
		l->changed = true;
	}
	else if (strncasecmp(data, "portal target ", 14) == 0)
	{
		struct portal_t *p = arg->edit;
		if (p == NULL) return;
		strncpy(p->target, data + 14, sizeof p->target);
		client_notify(c, "Portal target set");
		l->changed = true;
	}
	else if (strncasecmp(data, "portal target-level ", 20) == 0)
	{
		struct portal_t *p = arg->edit;
		if (p == NULL) return;
		strncpy(p->target_level, data + 20, sizeof p->target);
		client_notify(c, "Portal target-level set");
		l->changed = true;
	}
	else if (strcasecmp(data, "portal list") == 0)
	{
		unsigned i;
		for (i = 0; i < arg->portals; i++)
		{
			struct portal_t *p = &arg->portal[i];
			if (*p->name != '\0')
			{
				char buf[128];
				snprintf(buf, sizeof buf, TAG_YELLOW "Portal %s at %dx%dx%d goes to %s on %s", p->name, p->pos.x, p->pos.y, p->pos.z, p->target, p->target_level);
				client_notify(c, buf);
			}
		}
	}
}

static void portal_teleport(struct client_t *c, const char *target, struct portal_data_t *arg)
{
	char buf[128];
	struct portal_t *p = portal_get_by_name(target, arg, false);

	if (p == NULL)
	{
		snprintf(buf, sizeof buf, TAG_YELLOW "Portal target %s not found", target);
		client_notify(c, buf);
	}
	else if (p->pos.x == 0 && p->pos.y == 0 && p->pos.z == 0)
	{
		snprintf(buf, sizeof buf, TAG_YELLOW "Portal target %s has no position", target);
		client_notify(c, buf);
	}
	else
	{
//		snprintf(buf, sizeof buf, TAG_YELLOW "Teleporting to %s", target);
//		client_notify(c, buf);
		c->player->pos = p->pos;
		client_add_packet(c, packet_send_teleport_player(0xFF, &c->player->pos));
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

//			char buf[128];
//			snprintf(buf, sizeof buf, "Reached portal %s", p->name);
//			client_notify(c, buf);

			SetBit(c->player->flags, 7);

			/* Portal is exit only */
			if (*p->target == '\0') return;
			if (*p->target_level == '\0')
			{
				portal_teleport(c, p->target, arg);
			}
			else
			{
				/* Switch level */
				struct level_t *l2;
				if (level_get_by_name(p->target_level, &l2))
				{
					if (player_change_level(c->player, l2))
					{
						c->player->hook_data = p->target;
						level_send(c);
					}
				}
			}
			return;
		}
	}

	if (HasBit(c->player->flags, 7))
	{
//		client_notify(c, "Moved out of portal");
		c->player->flags ^= 1 << 7;
	}
}

static void portal_handle_spawn(struct level_t *l, struct client_t *c, char *data, struct portal_data_t *arg)
{
	SetBit(c->player->flags, 7);
	if (data == NULL) return;
	portal_teleport(c, data, arg);
}

static void portal_level_hook(int event, struct level_t *l, struct client_t *c, void *data, struct level_hook_data_t *arg)
{
	switch (event)
	{
		case EVENT_CHAT: portal_handle_chat(l, c, data, arg->data); break;
		case EVENT_MOVE: portal_handle_move(l, c, *(int *)data, arg->data); break;
		case EVENT_SPAWN: portal_handle_spawn(l, c, data, arg->data); break;
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
