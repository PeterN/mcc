#include "block.h"
#include "cuboid.h"
#include "client.h"
#include "level.h"
#include "player.h"

struct cuboid_list_t s_cuboids;

void cuboid_process(void)
{
	unsigned i;
	for (i = 0; i < s_cuboids.used; i++)
	{
		struct cuboid_t *c = &s_cuboids.items[i];
		int max = g_server.cuboid_max;

		if (c->srclevel != NULL)
		{
			/* Check mutexes when copying levels */
			if (pthread_mutex_trylock(&c->level->mutex) != 0) continue;
			pthread_mutex_unlock(&c->level->mutex);

			if (pthread_mutex_trylock(&c->srclevel->mutex) != 0) continue;
			pthread_mutex_unlock(&c->srclevel->mutex);

			/* Size not known, fetch it */
			if (c->ex == 0)
			{
				c->ex = (c->srclevel->x < c->level->x ? c->srclevel->x : c->level->x) - 1;
				c->ey = (c->srclevel->y < c->level->y ? c->srclevel->y : c->level->y) - 1;
				c->ez = (c->srclevel->z < c->level->z ? c->srclevel->z : c->level->z) - 1;
				c->cy = c->ey;
			}
		}

		while (max)
		{
			unsigned index = level_get_index(c->level, c->cx, c->cy, c->cz);
			struct block_t *b = &c->level->blocks[index];
			enum blocktype_t bt = b->type;

			enum blocktype_t pt1 = convert(c->level, index, b);

			if (c->old_type == BLOCK_INVALID || bt == c->old_type)
			{
				if (c->owner_is_op || b->owner == 0 || b->owner == c->owner)
				{
					delete(c->level, index, b);

					bool oldphysics = b->physics;

					/* Handle physics */
					if (c->srclevel != NULL)
					{
						unsigned index2 = level_get_index(c->srclevel, c->cx, c->cy, c->cz);
						*b = c->srclevel->blocks[index2];
					}
					else
					{
						b->type = c->new_type;
						b->data = 0;
						/* Set owner to none if placing air, unless fixed */
						b->owner = (c->new_type == AIR && !c->fixed) ? 0 : c->owner;
						b->fixed = c->fixed;
						b->physics = blocktype_has_physics(c->new_type);
					}

					if (oldphysics != b->physics)
					{
						if (oldphysics) physics_list_del_item(&c->level->physics, index);
						if (b->physics) physics_list_add(&c->level->physics, index);
					}

					enum blocktype_t pt2 = convert(c->level, index, b);

					if (!c->level->instant && pt1 != pt2)
					{
						unsigned j;
						for (j = 0; j < s_clients.used; j++)
						{
							struct client_t *client = s_clients.items[j];
							if (client->player == NULL) continue;
							if (client->player->level == c->level)
							{
								client_add_packet(client, packet_send_set_block(c->cx, c->cy, c->cz, pt2));
							}
						}

						max--;
					}

					c->count++;
					c->level->changed = true;
				}
			}

			c->cz++;
			if (c->cz > c->ez)
			{
				c->cz = c->sz;
				c->cx++;
				if (c->cx > c->ex)
				{
					c->cx = c->sx;
					c->cy--;
					if (c->cy < c->sy)
					{
						if (c->srclevel != NULL)
						{
							c->level->no_changes = 0;
							c->level->spawn = c->srclevel->spawn;
							level_notify_all(c->level, TAG_YELLOW "Level finished loading");
						}
						printf("ok: %d blocks changed\n", c->count);
						cuboid_list_del_index(&s_cuboids, i);
						return;
					}
				}
			}
		}
	}
}

void cuboid_remove_for_level(struct level_t *l)
{
	unsigned i;
	for (i = 0; i < s_cuboids.used; i++)
	{
		if (s_cuboids.items[i].level == l)
		{
			cuboid_list_del_index(&s_cuboids, i);
			/* Need to restart list */
			i = -1;
		}
	}
}
