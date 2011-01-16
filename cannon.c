#include <math.h>
#include "block.h"
#include "client.h"
#include "level.h"
#include "player.h"

static enum blocktype_t s_cannon;
static enum blocktype_t s_cannon_ball;

static enum blocktype_t convert_cannon(struct level_t *level, unsigned index, const struct block_t *block)
{
	return DARKGREY;
}

static void physics_cannon(struct level_t *level, unsigned index, const struct block_t *block)
{
	/* Decrement reload counter */
	if (block->data > 0)
	{
		level_addupdate(level, index, BLOCK_INVALID, block->data - 1);
	}
}

static enum blocktype_t convert_cannon_ball(struct level_t *level, unsigned index, const struct block_t *block)
{
	return OBSIDIAN;
}

struct cannons
{
	int tick;
	struct
	{
		int active;
		float x, y, z;
		float h; /* Heading */
		float vel_h;
		float vel_v;
		unsigned loc;
		//struct block_t b[4];
	} c[MAX_CLIENTS_PER_LEVEL];
};

static void cannons_handle_tick(struct level_t *l, struct client_t *c, void *data, struct cannons *cg)
{
	cg->tick++;
	if (cg->tick % 2 != 0) return;

	int i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		if (!cg->c[i].active) continue;

		float dx = cos(cg->c[i].h) * cg->c[i].vel_h;
		float dz = sin(cg->c[i].h) * cg->c[i].vel_h;

		cg->c[i].x += dx;
		cg->c[i].z += dz;
		cg->c[i].y += cg->c[i].vel_v;

		//cg->c[i].vel_h -= 0.001;
		cg->c[i].vel_v -= 2.0f / 32.0f;

		bool valid = true;
		if (!level_valid_xyz(l, cg->c[i].x, cg->c[i].y, cg->c[i].z))
		{
			if (cg->c[i].y < l->y || !level_valid_xyz(l, cg->c[i].x, l->y - 1, cg->c[i].z))
			{
				cg->c[i].active = false;
			}
			valid = false;
		}

		unsigned loc = level_get_index(l, cg->c[i].x, cg->c[i].y, cg->c[i].z);
		if (loc == cg->c[i].loc) continue;

		/* Moved: */
		if (cg->c[i].loc != -1) level_addupdate(l, cg->c[i].loc, AIR, 0);

		if (valid)
		{
			if (l->blocks[loc].type == AIR || l->blocks[loc].type == WATER || l->blocks[loc].type == WATERSTILL)
			{
				level_addupdate(l, loc, s_cannon_ball, 0);
				cg->c[i].loc = loc;
			}
			else
			{
				level_addupdate(l, loc, blocktype_get_by_name("active_tnt"), 0x501);
				cg->c[i].active = false;
				cg->c[i].loc = -1;
			}
		}
		else
		{
			cg->c[i].loc = -1;
		}

		printf("Location %f %f %f\n", cg->c[i].x, cg->c[i].y, cg->c[i].z);
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

		/* Start at the centre of the block */
		cg->c[i].x = be->x + 0.5f;
		cg->c[i].y = be->y + 0.5f;
		cg->c[i].z = be->z + 0.5f;

		cg->c[i].h = (c->player->pos.h - 64) * M_PI / 128;

		float p = c->player->pos.p * M_PI / 128;
		float f = 2.0f;

		cg->c[i].vel_h = f * cos(p);
		cg->c[i].vel_v = -f * sin(p);
		cg->c[i].active = 1;

		client_notify(c, TAG_YELLOW "Fire!");

		level_addupdate(l, index, BLOCK_INVALID, 40);
	}
}

static bool cannons_level_hook(int event, struct level_t *l, struct client_t *c, void *data, struct level_hook_data_t *arg)
{
	switch (event)
	{
		case EVENT_TICK:
			cannons_handle_tick(l, c, data, arg->data);
			break;

//		case EVENT_MOVEi:
//			cannons_handle_move(l, c, data, arg->data);
//			break;

		case EVENT_BLOCK:
			cannons_handle_block(l, c, data, arg->data);
			break;

		case EVENT_INIT:
			if (arg->size != 0) free(arg->data);
			arg->size = sizeof (struct cannons);
			arg->data = calloc(1, arg->size);
			break;

		case EVENT_DEINIT:
			if (l == NULL) break;
			free(arg->data);
			arg->size = 0;
			arg->data = NULL;
			break;
	}

	return false;
}

void module_init(void **data)
{
	s_cannon = register_blocktype(BLOCK_INVALID, "cannon", RANK_ADV_BUILDER, &convert_cannon, NULL, NULL, &physics_cannon, false, false, false);
	s_cannon_ball = register_blocktype(BLOCK_INVALID, "cannon_ball", RANK_ADMIN, &convert_cannon_ball, NULL, NULL, NULL, false, false, false);
	register_level_hook_func("cannons", &cannons_level_hook);
}

void module_deinit(void *data)
{
	deregister_blocktype(s_cannon);
	deregister_blocktype(s_cannon_ball);
	deregister_level_hook_func("cannons");
}
