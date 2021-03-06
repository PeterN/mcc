#include <math.h>
#include "block.h"
#include "client.h"
#include "level.h"
#include "player.h"

static enum blocktype_t s_cannon;
static enum blocktype_t s_cannon_ball;

static enum blocktype_t s_active_tnt;
static enum blocktype_t s_explosion;

static struct block_t s_block;

static enum blocktype_t convert_cannon(struct level_t *level, unsigned index, const struct block_t *block)
{
	return DARKGREY;
}

static void physics_cannon(struct level_t *level, unsigned index, const struct block_t *block)
{
	/* Decrement reload counter */
	if (block->data > 0)
	{
		level_addupdate(level, index, block->type, block->data - 1);
	}
}

static enum blocktype_t convert_cannon_ball(struct level_t *level, unsigned index, const struct block_t *block)
{
	return OBSIDIAN;
}

struct cannons
{
	int16_t maxy;
	struct
	{
		int active;
		float x, y, z;
		float h; /* Heading */
		float vel_h;
		float vel_v;
		unsigned loc;
		unsigned origin;
		unsigned owner;
	} c[MAX_CLIENTS_PER_LEVEL];
	float p[MAX_CLIENTS_PER_LEVEL];
};

static void cannons_handle_tick(struct level_t *l, struct client_t *c, void *data, struct cannons *cg)
{
	int i, j;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		if (!cg->c[i].active) continue;

		bool hit = false;
		unsigned loc = cg->c[i].loc;

		for (j = 0; j < 10; j++)
		{
			float dx = cos(cg->c[i].h) * cg->c[i].vel_h * 0.1;
			float dz = sin(cg->c[i].h) * cg->c[i].vel_h * 0.1;

			cg->c[i].x += dx;
			cg->c[i].z += dz;
			cg->c[i].y += cg->c[i].vel_v * 0.1;

			if (!level_valid_xyz(l, cg->c[i].x, cg->c[i].y > cg->maxy ? cg->maxy : cg->c[i].y, cg->c[i].z))
			{
				loc = -1;
				cg->c[i].active = false;
				break;
			}
			else if (cg->c[i].y <= cg->maxy)
			{
				unsigned newloc = level_get_index(l, cg->c[i].x, cg->c[i].y, cg->c[i].z);
				if (newloc == loc) continue;

				if (l->blocks[newloc].type == AIR || l->blocks[newloc].type == s_explosion ||
					l->blocks[newloc].type == WATER || l->blocks[newloc].type == WATERSTILL)
				{
					loc = newloc;
				}
				else
				{
					hit = true;
					/* Hit something */
					break;
				}
			}
		}

		/* Don't place if the cannon ball is off limits. */
		if (cg->c[i].y > cg->maxy) loc = -1;

		/* Apply gravity */
		cg->c[i].vel_v -= 1.0f / 32.0f;

		/* Apply simplistic airdrag */
		float d = 0.995f;
		cg->c[i].vel_v *= d;
		cg->c[i].vel_h *= d;

		/* Moved: */
		if (hit)
		{
			level_addupdate_with_owner(l, loc, s_active_tnt, 0x401, cg->c[i].owner);
		}

		if (cg->c[i].loc != -1 && cg->c[i].loc != cg->c[i].origin && cg->c[i].loc != loc)
		{
			s_block.type = AIR;
			s_block.owner = cg->c[i].owner;
			s_block.physics = 1;
			level_change_block_force(l, &s_block, cg->c[i].loc);
			physics_list_update(l, cg->c[i].loc, 1);
		}

		if (hit)
		{
			cg->c[i].loc = -1;
			cg->c[i].active = false;
			break;
		}

		if (cg->c[i].loc != -1 && loc != cg->c[i].origin)
		{
			s_block.type = s_cannon_ball;
			s_block.owner = 0;
			s_block.physics = 0;
			delete(l, loc, &l->blocks[loc]);
			level_change_block_force(l, &s_block, loc);
			physics_list_update(l, loc, 0);
			cg->c[i].loc = loc;
			cg->c[i].origin = -1;
		}
	}
}

static void cannons_handle_block(struct level_t *l, struct client_t *c, struct block_event *be, struct cannons *cg)
{
	if (be->bt == s_cannon && be->nt == AIR)
	{
		be->nt = be->bt;

		unsigned index = level_get_index(l, be->x, be->y, be->z);
		if (l->blocks[index].data != 0)
		{
			client_notify(c, TAG_YELLOW "Cannot fire, reloading...");
			return;
		}

		/* Left click */
		int i = c->player->levelid;

		if (cg->c[i].active)
		{
			client_notify(c, TAG_YELLOW "Cannot fire, reloading...");
			return;
		}

		/* Start at the centre of the block */
		cg->c[i].x = be->x + 0.5f;
		cg->c[i].y = be->y + 0.5f;
		cg->c[i].z = be->z + 0.5f;
		cg->c[i].loc = level_get_index(l, cg->c[i].x, cg->c[i].y, cg->c[i].z);
		cg->c[i].origin = cg->c[i].loc;

		cg->c[i].h = (c->player->pos.h - 64) * M_PI / 128;

		float p = c->player->pos.p * M_PI / 128;
		float f = cg->p[i];

		cg->c[i].vel_h = f * cos(p);
		cg->c[i].vel_v = -f * sin(p);
		cg->c[i].active = 1;
		cg->c[i].owner = c->player->globalid;

		client_notify(c, TAG_YELLOW "Fire!");

		level_addupdate(l, index, be->bt, 40);
	}
}

static bool cannons_handle_chat(struct level_t *l, struct client_t *c, char *data, struct cannons *cg)
{
	if (strcasecmp(data, "cp") == 0)
	{
		char buf[64];
		snprintf(buf, sizeof buf, TAG_YELLOW "Power at %d", (int)(cg->p[c->player->levelid] * 10));
		client_notify(c, buf);
		return true;
	}
	else if (strncasecmp(data, "cp ", 3) == 0)
	{
		char *endp;
		int i = strtol(data + 3, &endp, 10);
		if (*endp != '\0' || i > 50 || i < 0)
		{
			client_notify(c, TAG_YELLOW "Power must be between 0 and 50");
		}
		else
		{
			cg->p[c->player->levelid] = i * 0.1f;
			char buf[64];
			snprintf(buf, sizeof buf, TAG_YELLOW "Power at %d", i);
			client_notify(c, buf);
		}
		return true;
	}
	else if (strncasecmp(data, "cannon maxy ", 12) == 0)
	{
		char buf[64];
		char *endp;
		int i = strtol(data + 12, &endp, 10);
		if (*endp != '\0' || i < 0 || i >= l->y)
		{
			snprintf(buf, sizeof buf, TAG_YELLOW "MaxY must be between 0 and %d", l->y - 1);
		}
		else
		{
			cg->maxy = i;
			snprintf(buf, sizeof buf, TAG_YELLOW "MaxY set to %d", i);
		}
		client_notify(c, buf);
		return true;
	}

	return false;
}

static bool cannons_level_hook(int event, struct level_t *l, struct client_t *c, void *data, struct level_hook_data_t *arg)
{
	switch (event)
	{
		case EVENT_TICK:
			cannons_handle_tick(l, c, data, arg->data);
			break;

		case EVENT_CHAT:
			return cannons_handle_chat(l, c, data, arg->data);

		case EVENT_BLOCK:
			cannons_handle_block(l, c, data, arg->data);
			break;

		case EVENT_SPAWN:
		{
			struct cannons *cg = arg->data;
			cg->p[c->player->levelid] = 3.0f;
			break;
		}

		case EVENT_INIT:
			if (arg->size != sizeof (struct cannons))
			{
				free(arg->data);

				arg->size = sizeof (struct cannons);
				arg->data = calloc(1, arg->size);

				struct cannons *cg = arg->data;
				cg->maxy = l->y - 1;
			}
			break;
	}

	return false;
}

void module_init(void **data)
{
	s_cannon = register_blocktype(BLOCK_INVALID, "cannon", RANK_ADV_BUILDER, &convert_cannon, NULL, NULL, &physics_cannon, false, false, false);
	s_cannon_ball = register_blocktype(BLOCK_INVALID, "cannon_ball", RANK_ADMIN, &convert_cannon_ball, NULL, NULL, NULL, false, false, false);

	s_active_tnt = blocktype_get_by_name("active_tnt");
	s_explosion  = blocktype_get_by_name("explosion");

	register_level_hook_func("cannons", &cannons_level_hook);
}

void module_deinit(void *data)
{
	deregister_blocktype(s_cannon);
	deregister_blocktype(s_cannon_ball);
	deregister_level_hook_func("cannons");
}
