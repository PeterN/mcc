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
	struct portal_t *edit;
	struct portal_t portal[];
};

static struct portal_t *portal_get_by_name(const char *name, struct portal_data_t *arg, struct level_hook_data_t *ld, bool create)
{
	int i;

	/* Find existing portal */
	for (i = 0; i < arg->portals; i++)
	{
		if (strncasecmp(arg->portal[i].name, name, sizeof arg->portal[i].name - 1) == 0) return &arg->portal[i];
	}

	if (!create) return NULL;

	/* Find empty slot for new portal */
	for (i = 0; i < arg->portals; i++)
	{
		if (*arg->portal[i].name == '\0')
		{
			memset(&arg->portal[i], 0, sizeof arg->portal[i]);
			snprintf(arg->portal[i].name, sizeof arg->portal[i].name, "%s", name);
			return &arg->portal[i];
		}
	}

	/* Create new slot */
	ld->size = sizeof (struct portal_data_t) + sizeof (struct portal_t) * (arg->portals + 1);
	arg = realloc(ld->data, ld->size);
	if (arg == NULL) return NULL;
	ld->data = arg;

	memset(&arg->portal[i], 0, sizeof arg->portal[i]);
	snprintf(arg->portal[i].name, sizeof arg->portal[i].name, "%s", name);
	arg->portals++;
	return &arg->portal[i];
}

static void portal_handle_chat(struct level_t *l, struct client_t *c, char *data, struct portal_data_t *arg, struct level_hook_data_t *ld)
{
	if (!level_user_can_build(l, c->player)) return;

	if (strncasecmp(data, "portal edit ", 12) == 0)
	{
		struct portal_t *p = portal_get_by_name(data + 12, arg, ld, true);
		if (p == NULL) return;
		arg = ld->data;
		arg->edit = p;
		char buf[128];
		snprintf(buf, sizeof buf, TAG_YELLOW "Editing portal %s", data + 12);
		client_notify(c, buf);
	}
	else if (strncasecmp(data, "portal delete ", 14) == 0)
	{
		struct portal_t *p = portal_get_by_name(data + 14, arg, ld, false);
		if (p == NULL) return;
		memset(p, 0, sizeof *p);
		client_notify(c, TAG_YELLOW "Portal deleted");
		if (arg->edit == p) arg->edit = NULL;
		l->changed = true;
	}
	else if (strncasecmp(data, "portal rename ", 14) == 0)
	{
		struct portal_t *p = arg->edit;
		if (p == NULL) return;
		snprintf(p->name, sizeof p->name, data + 14);
		client_notify(c, TAG_YELLOW "Portal renamed");
		l->changed = true;
	}
	else if (strcasecmp(data, "portal place") == 0)
	{
		struct portal_t *p = arg->edit;
		if (p == NULL) return;
		p->pos = c->player->pos;
		client_notify(c, TAG_YELLOW "Portal position set");
		l->changed = true;
	}
	else if (strcasecmp(data, "portal no target") == 0)
	{
		struct portal_t *p = arg->edit;
		if (p == NULL) return;
		memset(p->target, 0, sizeof p->target);
		client_notify(c, TAG_YELLOW "Portal target cleared");
		l->changed = true;
	}
	else if (strncasecmp(data, "portal target ", 14) == 0)
	{
		struct portal_t *p = arg->edit;
		if (p == NULL) return;
		snprintf(p->target, sizeof p->target, "%s", data + 14);
		client_notify(c, TAG_YELLOW "Portal target set");
		l->changed = true;
	}
	else if (strcasecmp(data, "portal no target-level") == 0)
	{
		struct portal_t *p = arg->edit;
		if (p == NULL) return;
		memset(p->target_level, 0, sizeof p->target_level);
		client_notify(c, TAG_YELLOW "Portal target-level cleared");
		l->changed = true;
	}
	else if (strncasecmp(data, "portal target-level ", 20) == 0)
	{
		struct portal_t *p = arg->edit;
		if (p == NULL) return;
		snprintf(p->target_level, sizeof p->target_level, "%s", data + 20);
		client_notify(c, TAG_YELLOW "Portal target-level set");
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

static void portal_teleport(struct client_t *c, const char *target, struct portal_data_t *arg, bool instant)
{
	char buf[128];
	struct portal_t *p = portal_get_by_name(target, arg, NULL, false);

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
		player_teleport(c->player, &p->pos, instant);
	}
}

static void portal_handle_move(struct level_t *l, struct client_t *c, int index, struct portal_data_t *arg)
{
	/* Changing levels, don't handle teleports */
	if (c->player->level != c->player->new_level) return;

//	char buf[64];
//	snprintf(buf, sizeof buf, "position on %s: %d %d %d\n", c->player->level->name, c->player->pos.x, c->player->pos.y, c->player->pos.z);
//	client_notify(c, buf);

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
			if (*p->target_level == '\0')
			{
				if (*p->target == '\0') return;
				portal_teleport(c, p->target, arg, true);
			}
			else
			{
				/* Switch level */
				struct level_t *l2;
				if (level_get_by_name(p->target_level, &l2))
				{
					if (player_change_level(c->player, l2))
					{
						if (*p->target != '\0') c->player->hook_data = p->target;
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
	portal_teleport(c, data, arg, false);
}

static void portal_level_hook(int event, struct level_t *l, struct client_t *c, void *data, struct level_hook_data_t *arg)
{
	switch (event)
	{
		case EVENT_CHAT: portal_handle_chat(l, c, data, arg->data, arg); break;
		case EVENT_MOVE: portal_handle_move(l, c, *(int *)data, arg->data); break;
		case EVENT_SPAWN: portal_handle_spawn(l, c, data, arg->data); break;
//		case EVENT_LOAD: portal->edit = NULL; break;
		case EVENT_INIT:
		{
			if (arg->size == 0)
			{
				LOG("Allocating new portal data on %s\n", l->name);
			}
			else
			{
				struct portal_data_t *pd = arg->data;
				if (arg->size == sizeof (struct portal_data_t) + sizeof (struct portal_t) * pd->portals)
				{
					LOG("Found data for %d portals on %s\n", pd->portals, l->name);
					unsigned i;
					for (i = 0; i < pd->portals; i++)
					{
						struct portal_t *p = &pd->portal[i];
						if (*p->name != '\0') continue;
						if (p->pos.x == 0 && p->pos.y == 0 && p->pos.z == 0) continue;
						snprintf(p->name, sizeof p->name, "unamed");
					}

					/* Ensure portal edit isn't set after loading */
					pd->edit = NULL;
					break;
				}

				LOG("Found invalid portal data on %s, erasing\n", l->name);
				free(arg->data);
			}

			arg->size = sizeof (struct portal_data_t);
			arg->data = calloc(1, arg->size);
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
