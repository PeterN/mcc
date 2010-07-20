#include "block.h"
#include "cuboid.h"
#include "client.h"
#include "level.h"
#include "player.h"

struct cuboid_list_t s_cuboids;

void cuboid_process()
{
	int i;
	for (i = 0; i < s_cuboids.used; i++)
	{
		struct cuboid_t *c = &s_cuboids.items[i];
		int max = 50;

		while (max)
		{
			unsigned index = level_get_index(c->level, c->cx, c->cy, c->cz);
			struct block_t *b = &c->level->blocks[index];
			enum blocktype_t bt = b->type;

			enum blocktype_t pt1 = convert(c->level, index, b);

			if (bt != c->new_type && (c->old_type == -1 || bt == c->old_type))
			{
				bool oldphysics = b->physics;

				/* Handle physics */
				b->type = c->new_type;
				b->data = 0;
				/* Set owner to none if placing air, unless fixed */
				b->owner = (c->new_type == AIR && !c->fixed) ? 0 : c->owner;
				b->fixed = c->fixed;
				b->physics = blocktype_has_physics(c->new_type);

				if (oldphysics != b->physics)
				{
					if (oldphysics) physics_list_del_item(&c->level->physics, index);
					if (b->physics) physics_list_add(&c->level->physics, index);
				}

				enum blocktype_t pt2 = convert(c->level, index, b);

				if (pt1 != pt2)
				{
					int j;
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
						printf("ok: %d blocks changed\n", c->count);
						cuboid_list_del_index(&s_cuboids, i);
						return;
					}
				}
			}
		}
	}
}
