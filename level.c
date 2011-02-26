#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <limits.h>
#include <zlib.h>
#include <pthread.h>
#include <math.h>
#include "filter.h"
#include "level.h"
#include "level_worker.h"
#include "block.h"
#include "client.h"
#include "cuboid.h"
#include "faultgen.h"
#include "mcc.h"
#include "packet.h"
#include "perlin.h"
#include "player.h"
#include "playerdb.h"
#include "network.h"
#include "undodb.h"
#include "util.h"
#include "gettime.h"

struct level_list_t s_levels;

bool level_t_compare(struct level_t **a, struct level_t **b)
{
	return *a == *b;
}

bool level_init(struct level_t *level, int16_t x, int16_t y, int16_t z, const char *name, bool zero)
{
	if (zero)
	{
		memset(level, 0, sizeof *level);
		pthread_mutex_init(&level->mutex, NULL);
		pthread_mutex_init(&level->inuse_mutex, NULL);
		pthread_mutex_init(&level->hook_mutex, NULL);
		pthread_mutex_init(&level->physics_mutex, NULL);
	}

	if (name != NULL)
	{
		strncpy(level->name, name, sizeof level->name);
	}

	level->x = x;
	level->y = y;
	level->z = z;

	level->blocks = calloc(x * y * z, sizeof *level->blocks);
	if (level->blocks == NULL)
	{
		LOG("level_init: allocation of %zu bytes failed\n", x * y * z * sizeof *level->blocks);
		return false;
	}

	physics_list_init(&level->physics);

	return true;
}

void level_notify_all(struct level_t *level, const char *message)
{
	if (level == NULL) return;

	int i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		if (level->clients[i] == NULL) continue;
		client_notify(level->clients[i], message);
	}

	LOG("[%s] %s\n", level->name, message);
}

void level_set_block(struct level_t *level, struct block_t *block, unsigned index)
{
/*	bool old_phys = block_has_physics(&level->blocks[index]);
	bool new_phys = block_has_physics(block);*/

	level->blocks[index] = *block;

/*	if (new_phys != old_phys)
	{
		if (new_phys)
		{
			physics_list_add(&level->physics, index);
		}
		else
		{
			physics_list_del_item(&level->physics, index);
		}
	}*/
}

void level_set_block_if(struct level_t *level, struct block_t *block, unsigned index, enum blocktype_t type)
{
	if (level->blocks[index].type == type)
	{
		level_set_block(level, block, index);
	}
}

int level_get_new_id(struct level_t *level, struct client_t *c)
{
	int i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		if (level->clients[i] == NULL)
		{
			level->clients[i] = c;
			return i;
		}
	}

	return -1;
}

int level_get_new_npc_id(struct level_t *level, struct npc *npc)
{
	int i;
	for (i = 0; i < MAX_NPCS_PER_LEVEL; i++)
	{
		if (level->npcs[i] == NULL)
		{
			level->npcs[i] = npc;
			return i;
		}
	}

	return -1;
}

bool level_user_can_visit(const struct level_t *l, const struct player_t *p)
{
	if (p->rank >= l->rankvisit) return true;

	unsigned i;
	for (i = 0; i < l->uservisit.used; i++)
	{
		if (l->uservisit.items[i] == p->globalid) return true;
	}

	return false;
}

bool level_user_can_build(const struct level_t *l, const struct player_t *p)
{
	if (p->rank >= l->rankbuild) return true;

	unsigned i;
	for (i = 0; i < l->userbuild.used; i++)
	{
		if (l->userbuild.items[i] == p->globalid) return true;
	}

	return false;
}

bool level_user_can_own(const struct level_t *l, const struct player_t *p)
{
	if (p->rank >= l->rankown) return true;

	unsigned i;
	for (i = 0; i < l->userown.used; i++)
	{
		if (l->userown.items[i] == p->globalid) return true;
	}

	return false;
}

bool level_send(struct client_t *c)
{
	struct level_t *oldlevel = c->player->level;
	struct level_t *newlevel = c->player->new_level;

	if (newlevel == NULL)
	{
		static const char *start_levels[] = { "main", "hub", "hub2", "free2" };
		int i;

		for (i = 0; i < 4; i++)
		{
			if (!level_get_by_name(start_levels[i], &newlevel)) continue;
			if (level_get_new_id(newlevel, NULL) == -1) continue;
			break;
		}

		if (i == 4)
		{
			LOG("All start levels are full!\n");
			net_close(c, "All start levels full");
			return false;
		}

		c->player->new_level = newlevel;
	}

	unsigned length = newlevel->x * newlevel->y * newlevel->z;
	unsigned x;
	int i;
	z_stream z;

	/* If we can't lock the mutex then the thread is already locked */
	if (pthread_mutex_trylock(&newlevel->mutex))
	{
		if (!c->waiting_for_level)
		{
			client_notify(c, "Please wait for level operation to complete");
			c->waiting_for_level = true;
		}
		return false;
	}
	pthread_mutex_unlock(&newlevel->mutex);

	if (!level_user_can_visit(newlevel, c->player))
	{
		c->waiting_for_level = false;
		c->player->new_level = oldlevel;
		client_notify(c, "You can't join this level");
		return false;
	}

	int levelid;
	if (oldlevel == newlevel)
	{
		levelid = c->player->levelid;
	}
	else
	{
		levelid = level_get_new_id(newlevel, c);
		if (levelid == -1)
		{
			c->waiting_for_level = false;
			c->player->new_level = oldlevel;
			client_notify(c, "Uh, level is full, sorry...");
			return false;
		}
	}

	memset(&z, 0, sizeof z);
	if (deflateInit2(&z, 5, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK)
	{
		LOG("level_send: deflateInit2() failed\n");
		return false;
	}

	uint8_t *buffer = malloc(4 + length);
	if (buffer == NULL)
	{
		LOG("level_send: Unable to allocate %u bytes\n", 4 + length);
		deflateEnd(&z);
		return false;
	}

	c->sending_level = true;

	uint8_t *bufp = buffer;
	*bufp++ = (length >> 24) & 0xFF;
	*bufp++ = (length >> 16) & 0xFF;
	*bufp++ = (length >>  8) & 0xFF;
	*bufp++ =  length        & 0xFF;

	/* Serialize map data */
	for (x = 0; x < length; x++)
	{
		if (c->player->filter > 0)
		{
			*bufp++ = (newlevel->blocks[x].owner == c->player->filter) ? convert(newlevel, x, &newlevel->blocks[x]) : AIR;
		}
		else
		{
			*bufp++ = convert(newlevel, x, &newlevel->blocks[x]);
		}
	}

	if (oldlevel != NULL)
	{
		if (oldlevel != newlevel)
		{
			char buf[64];

			call_level_hook(EVENT_DESPAWN, oldlevel, c, NULL);

			/* Despawn this user for all users */
			if (!c->hidden) client_send_despawn(c->player->client, false);
			oldlevel->clients[c->player->levelid] = NULL;

			if (!c->hidden)
			{
				snprintf(buf, sizeof buf, TAG_WHITE "=%s" TAG_YELLOW " moved to '%s'", c->player->colourusername, newlevel->name);
				call_hook(HOOK_CHAT, buf);
				level_notify_all(oldlevel, buf);
				level_notify_all(newlevel, buf);
			}

			/* Reset the player's block mode */
			c->player->mode = MODE_NORMAL;
		}

		/* Despawn users for this user */
		client_despawn_players(c);

		for (i = 0; i < MAX_NPCS_PER_LEVEL; i++)
		{
			if (oldlevel->npcs[i] != NULL)
			{
				client_add_packet(c, packet_send_despawn_player(MAX_CLIENTS_PER_LEVEL + i));
			}
		}
	}

	c->player->level = newlevel;
	c->player->levelid = levelid;

	client_add_packet(c, packet_send_level_initialize());

	uint8_t outbuf[1024];
	length += 4;

	z.next_in = buffer;
	z.avail_in = length;

	do
	{
		z.next_out = outbuf;
		z.avail_out = sizeof outbuf;

		int r = deflate(&z, Z_FINISH);
		unsigned n = sizeof outbuf - z.avail_out;
		if (n != 0)
		{
			client_add_packet(c, packet_send_level_data_chunk(n, outbuf, (length - z.avail_in) * 100 / length));
		}

		if (r == Z_STREAM_END) break;
		if (r != Z_OK)
		{
			free(buffer);
			return false;
		}
	}
	while (z.avail_in > 0 || z.avail_out == 0);

	free(buffer);

	deflateEnd(&z);

	client_add_packet(c, packet_send_level_finalize(newlevel->x, newlevel->y, newlevel->z));

	if (oldlevel != newlevel)
	{
		c->player->pos = newlevel->spawn;
		c->player->lastpos = newlevel->spawn;
		c->player->teleport = true;
		call_level_hook(EVENT_SPAWN, newlevel, c, (void*)c->player->hook_data);
		c->player->hook_data = NULL;
	}

	client_add_packet(c, packet_send_spawn_player(0xFF, c->player->username, &c->player->pos));

	client_spawn_players(c);

	for (i = 0; i < MAX_NPCS_PER_LEVEL; i++)
	{
		if (newlevel->npcs[i] != NULL)
		{
			client_add_packet(c, packet_send_spawn_player(MAX_CLIENTS_PER_LEVEL + i, newlevel->npcs[i]->name, &newlevel->npcs[i]->pos));
		}
	}

	if (oldlevel != newlevel)
	{
		if (!c->hidden)
		{
			client_send_spawn(c, false);
		}
	}

	c->waiting_for_level = false;
	c->sending_level = false;

	return true;
}

extern void level_gen_mcsharp(struct level_t *level, const char *type);
void level_prerun(struct level_t *l);

void *level_gen_thread(struct level_t *level, const char *type)
{
	int i;

	pthread_mutex_lock(&level->mutex);

	struct block_t block;
	int x, y, z;
	int mx = level->x;
	int my = level->y;
	int mz = level->z;
	char buf[64];

	memset(&block, 0, sizeof block);

	memset(level->blocks, 0, sizeof *level->blocks * mx * my * mz);

	if (!strcmp(type, "flat") || !strcmp(type, "adminium"))
	{
		bool adminium = !strcmp(type, "adminium");

		int h = my / 2;

		for (z = 0; z < mz; z++)
		{
			for (x = 0; x < mx; x++)
			{
				for (y = 0; y < h; y++)
				{
					block.type = (y < h - 5) ? ROCK : (y < h - 1) ? DIRT : GRASS;
					if (adminium && y == h - 2) block.type = ADMINIUM;
					level_set_block(level, &block, level_get_index(level, x, y, z));
				}
			}
		}
	}
	else if (!strcmp(type, "pixel"))
	{
		block.type = WHITE;

		for (z = 0; z < mz; z++)
			for (x = 0; x < mx; x++)
				level_set_block(level, &block, level_get_index(level, x, 0, z));

		for (y = 0; y < my; y++)
		{
			for (z = 0; z < mz; z++)
			{
				level_set_block(level, &block, level_get_index(level, 0, y, z));
				level_set_block(level, &block, level_get_index(level, mx - 1, y, z));
			}
			for (x = 0; x < mx; x++)
			{
				level_set_block(level, &block, level_get_index(level, x, y, 0));
				level_set_block(level, &block, level_get_index(level, x, y, mz - 1));
			}
		}
	}
	else if (!strcmp(type, "old"))
	{
		struct faultgen_t *fg = faultgen_init(mx, mz);
		if (fg == NULL)
		{
			goto level_error;
		}
		struct filter_t *ft = filter_init(mx, mz);
		if (ft == NULL)
		{
			faultgen_deinit(fg);
			goto level_error;
		}
//		const float *hm1 = faultgen_map(fg);
		faultgen_create(fg, false);
		filter_process(ft, faultgen_map(fg));
		faultgen_deinit(fg);
		const float *hm1 = filter_map(ft);


		struct perlin_t *pp = perlin_init(mx, mz, rand(), 0.250 * (rand() % 6), 6);
		if (pp == NULL)
		{
			filter_deinit(ft);
			goto level_error;
		}
		const float *hm2 = perlin_map(pp);

		perlin_noise(pp);

//		float *hm = malloc((mx + 1) * (mz + 1) * sizeof *hm);
/*		float *cmh = malloc((mx + 1) * (mz + 1) * sizeof *cmh);
		float *cmd = malloc((mx + 1) * (mz + 1) * sizeof *cmd);*/

//		level_gen_heightmap(hm, mx, mz, type - 2);
		//level_smooth_slopes(hm, mx, mz, my);

		int height_range = my / 2;
		int sea_height = my / 2;

		LOG("levelgen: creating ground\n");

		float avg = 0;
		for (i = 0; i < mx * mz; i++)
		{
			avg += hm1[i];
		}
		avg /= mx * mz;

		LOG("levelgen: average height %f\n", avg);

		int dmx = mx;

		for (z = 0; z < mz; z++)
		{
			for (x = 0; x < mx; x++)
			{
				int h = (hm1[x + z * dmx] - avg * 0.5) * height_range + my / 2;
				float hm2p = hm2[x + z * dmx];
				int rh = 5;
				if (hm2p < 0.25) rh = hm2p * 20 - 2;

				for (y = 0; y < h && y < my; y++)
				{
					block.type = (y < h - rh) ? ROCK : (y < h - 1) ? DIRT : (y <= sea_height) ? SAND : GRASS;
					level_set_block(level, &block, level_get_index(level, x, y, z));
				}

				block.type = WATER;
				for (; y < sea_height && y < my; y++)
				{
					level_set_block(level, &block, level_get_index(level, x, y, z));
				}
			}
		}

//		block.type = AIR;
/*
		for (i = 0; i < 16; i++)
		{
			level_gen_heightmap(cmh, mx, mz, type - 2);
			level_gen_heightmap(cmd, mx, mz, type - 2);

			int base = cmd[0] * my;

			switch (i % 8)
			{
				case 0: block.type = AIR; break;
				case 1: block.type = ROCK; break;
				case 2: block.type = DIRT; break;
				case 3: block.type = GRAVEL; break;
				case 4: block.type = SAND; break;
				case 5: block.type = GOLDROCK; break;
				case 6: block.type = IRONROCK; break;
				case 7: block.type = COAL; break;
			}

			for (z = 0; z < mz; z++)
			{
				for (x = 0; x < mx; x++)
				{
					//int h = hm[x + z * dmx] * my / 2 + my / 3;
					float ch = cmh[x + z * dmx];
					float cd = cmd[x + z * dmx];
					int h = hm[x + z * dmx] * my / 2 + my / 4;
					//if (fabsf(ch) < 0.25f)
					{
						//block.type = i % 3 ? AIR : ROCK;
						//int cdi = h - cd * my / 4;
						//int chi = cdi + (ch - 0.5f) * my / 4;
						//int cdi = base + cd * my / 8;// / 1.5f;
						//int chi = ch * my / 4 + cdi;//di + (ch - 0.5f) * my / 3.0f;
						int cdi = cd * h * 1.2;
						int chi = cdi + ch * h * 0.25;
						if (cdi > chi) {
							cdi = cdi ^ chi;
							chi = cdi ^ chi;
							cdi = cdi ^ chi;
						}
						for (y = cdi; y < chi; y++)
						{
							unsigned index;
							if (y < 0 || y >= my) continue;
							index = level_get_index(level, x, y, z);
							//if (block.type == ROCK && (level->blocks[index].type == AIR || level->blocks[index].type == WATER))
							//{
								level_set_block(level, &block, index);
							//}
						}
					}
				}
			}
		}
*/
//		free(hm);
/*		free(cmd);
		free(cmh);*/

		perlin_deinit(pp);
		filter_deinit(ft);
//		faultgen_deinit(fg);

		LOG("levelgen: flooding sealevel\n");

		block.type = WATER;
		for (i = 0; i < 0; i++)
		{
			for (z = 0; z < mz; z++)
			{
				for (x = 0; x < mx; x++)
				{
					for (y = my / 2 - 1; y > 1; y--)
					{
						unsigned index = level_get_index(level, x, y, z);
						if (y > my / 2 - 3 && level->blocks[index].type == AIR)
						{
							if (x == 0 || x == mx - 1 || z == 0 || z == mz - 1)
							{
								level_set_block(level, &block, index);
							}
						}
						else if (level->blocks[index].type == WATER)
						{
							level_set_block_if(level, &block, level_get_index(level, x, y - 1, z), AIR);
							if (x > 0) level_set_block_if(level, &block, level_get_index(level, x - 1, y, z), AIR);
							if (x < mx - 1) level_set_block_if(level, &block, level_get_index(level, x + 1, y, z), AIR);
							if (z > 0) level_set_block_if(level, &block, level_get_index(level, x, y, z - 1), AIR);
							if (z < mz - 1) level_set_block_if(level, &block, level_get_index(level, x, y, z + 1), AIR);
						}
					}
				}
			}
		}

		LOG("levelgen: placing grass\n");

		/* Grow grass on soil */
		for (z = 0; z < mz; z++)
		{
			for (x = 0; x < mx; x++)
			{
				for (y = my - 1; y > 0; y--)
				{
					unsigned index = level_get_index(level, x, y, z);
					if (level->blocks[index].type == DIRT)
					{
						level->blocks[index].type = GRASS;
						break;
					}
					if (level->blocks[index].type != AIR) break;
				}
			}
		}
	}
	else
	{
		level_gen_mcsharp(level, type);
	}

	LOG("levelgen: setting spawn\n");

	level->spawn.x = mx * 16;
	level->spawn.z = mz * 16;
	for (y = my - 5; y > 0; y--)
	{
		unsigned index = level_get_index(level, mx / 2, y, mz / 2);
		if (level->blocks[index].type != AIR)
		{
			level->spawn.y = (y + 4) * 32;
			break;
		}
	}

	level->spawn.h = 0;
	level->spawn.p = 0;

	/*
	for (y = 0; y < my / 2; y++)
	{
		block.type = (y < my / 2 - 5) ? ROCK : (y < my / 2 - 1) ? DIRT : GRASS;
		for (x = 0; x < mx; x++)
		{
			for (z = 0; z < mz; z++)
			{
				level_set_block(level, &block, level_get_index(level, x, y, z));
			}
		}
	}
	for (i = 0; i < mx * mz / 64; i++)
	{
		x = rand() % mx;
		z = rand() % mz;
		block.type = rand() % ROCK_END;

		for (y = rand() % (my / 4); y < my; y++)
		{
			level_set_block(level, &block, level_get_index(level, x, y, z));
		}
	}

	for (i = 0; i < mx * mz; i++)
	{
		x = rand() % mx;
		z = rand() % mz;
		y = rand() % my;
		block.type = rand() % ROCK_END;
		level_set_block(level, &block, level_get_index(level, x, y, z));
	}*/

	level->physics.used = 0;

	LOG("levelgen: activating physics\n");

	/* Activate physics */
	int count = mx * my * mz;
	for (i = 0; i < count; i++)
	{
		struct block_t *b = &level->blocks[i];
		b->physics = blocktype_has_physics(b->type);
		if (b->physics) physics_list_add(&level->physics, i);
	}

	LOG("levelgen: %llu physics blocks, prerunning\n", (long long unsigned)level->physics.used);

	level_prerun(level);

	LOG("levelgen: %llu physics blocks remaining\n", (long long unsigned)level->physics.used);

	LOG("levelgen: complete\n");

	level->changed = true;

	snprintf(buf, sizeof buf, "Created level '%s'", level->name);
	net_notify_ops(buf);

	pthread_mutex_unlock(&level->mutex);

	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		struct client_t *c = level->clients[i];
		if (c != NULL) level_send_queue(c);
	}

	level_inuse(level, false);

	return NULL;

level_error:
	level->changed = true;
	level_inuse(level, false);

	snprintf(buf, sizeof buf, TAG_YELLOW "Level generation for %s failed\n", level->name);
	LOG(buf);

	pthread_mutex_unlock(&level->mutex);

	return NULL;
}

void level_gen(struct level_t *level, const char *type, int height_range, int sea_height)
{
	if (type == NULL || strlen(type) == 0)
	{
		LOG("Invalid level type specified\n");
		return;
	}

	if (!level_inuse(level, true)) return;

	level_make_queue(level, type);
}

void level_unload(struct level_t *level)
{
	LOG("Level '%s' unloaded\n", level->name);

	pthread_mutex_lock(&level->mutex);

	user_list_free(&level->userbuild);
	user_list_free(&level->uservisit);
	physics_list_free(&level->physics);
	physics_list_free(&level->physics2);
	block_update_list_free(&level->updates);

	undodb_close(level->undo);

	if (level->delete)
	{
		char filename[256];
		snprintf(filename, sizeof filename, "levels/%s.mcl", level->name);
		lcase(filename);
		unlink(filename);

		snprintf(filename, sizeof filename, "undo/%s.db", level->name);
		lcase(filename);
		unlink(filename);

		LOG("Level '%s' deleted\n", level->name);
	}

	int i;
	for (i = 0; i < MAX_HOOKS_PER_LEVEL; i++)
	{
		free(level->level_hook[i].data.data);
	}

	free(level->blocks);

	pthread_mutex_unlock(&level->mutex);

	free(level);
}

static void *level_load_thread_abort(struct level_t *level, const char *reason)
{
	LOG("Unable to load level %s: %s\n", level->name, reason);

	unsigned i;
	for (i = 0; i < s_clients.used; i++)
	{
		struct client_t *c = s_clients.items[i];
		if (c == NULL || c->player == NULL) continue;

		if (c->player->new_level == level)
		{
			c->player->new_level = c->player->level;
			c->waiting_for_level = false;

			LOG("Aborted level change for %s\n", c->player->username);

			client_notify(c, "Level change aborted...");
		}
	}

	pthread_mutex_unlock(&level->mutex);

	return NULL;
}

void *level_load_thread(void *arg)
{
	int i;
	struct level_t *l = arg;
	gzFile gz;

	char name[64];
	strncpy(name, l->name, sizeof name);

	char filename[64];
	snprintf(filename, sizeof filename, "levels/%s.%s", name, l->convert ? "lvl" : "mcl");
	lcase(filename);

	gz = gzopen(filename, "rb");
	if (gz == NULL) return level_load_thread_abort(l, "gzopen failed");

	if (l->convert)
	{
   		int16_t x, y, z;
		int16_t version;

		if (gzread(gz, &version, sizeof version) != sizeof version) return level_load_thread_abort(l, "version/x");
		if (version == 1874)
		{
			if (gzread(gz, &x, sizeof x) != sizeof x) return level_load_thread_abort(l, "x");
		}
		else
		{
			x = version;
		}
		if (gzread(gz, &z, sizeof z) != sizeof y) return level_load_thread_abort(l, "z");
		if (gzread(gz, &y, sizeof y) != sizeof z) return level_load_thread_abort(l, "y");
		if (!level_init(l, x, y, z, name, false)) return level_load_thread_abort(l, "level_init failed");
		if (gzread(gz, &l->spawn.x, sizeof l->spawn.x) != sizeof l->spawn.x) return level_load_thread_abort(l, "spawn.x");
		if (gzread(gz, &l->spawn.z, sizeof l->spawn.z) != sizeof l->spawn.z) return level_load_thread_abort(l, "spawn.z");
		if (gzread(gz, &l->spawn.y, sizeof l->spawn.y) != sizeof l->spawn.y) return level_load_thread_abort(l, "spawn.y");
		if (gzread(gz, &l->spawn.h, sizeof l->spawn.h) != sizeof l->spawn.h) return level_load_thread_abort(l, "spawn.h");
		if (gzread(gz, &l->spawn.p, sizeof l->spawn.p) != sizeof l->spawn.p) return level_load_thread_abort(l, "spawn.p");

		l->spawn.x = l->spawn.x * 32 + 16;
		l->spawn.y = l->spawn.y * 32 + 32;
		l->spawn.z = l->spawn.z * 32 + 16;

		if (version == 1874)
		{
			if (gzread(gz, &l->rankvisit, sizeof l->rankvisit) != sizeof l->rankvisit) return level_load_thread_abort(l, "rankvisit");
			if (gzread(gz, &l->rankbuild, sizeof l->rankbuild) != sizeof l->rankbuild) return level_load_thread_abort(l, "rankbuild");

			/* MCSharp uses a different set of permission values for levels for some reason */
			if (l->rankvisit <= 3) l->rankvisit++;
			if (l->rankbuild <= 3) l->rankbuild++;
			l->rankvisit = rank_convert(l->rankvisit);
			l->rankbuild = rank_convert(l->rankbuild);
			l->rankown   = RANK_OP;
		}
		else
		{
			l->rankvisit = RANK_GUEST;
			l->rankbuild = RANK_GUEST;
			l->rankown   = RANK_OP;
		}

		int s = x * y * z;
		uint8_t *blocks = malloc(s);
		if (blocks == NULL) return level_load_thread_abort(l, "malloc blocks failed");
		if (gzread(gz, blocks, s) != s)
		{
			free(blocks);
			return level_load_thread_abort(l, "blocks");
		}

		for (i = 0; i < s; i++)
		{
			l->blocks[i] = block_convert_from_mcs(blocks[i]);
		}

		free(blocks);

		int count = l->x * l->y * l->z;
		for (i = 0; i < count; i++)
		{
			struct block_t *b = &l->blocks[i];
			if (b->type == AIR || b->type == WATER || b->type == LAVA) continue;
			b->physics = blocktype_has_physics(b->type);
			if (b->physics) physics_list_add(&l->physics, i);
		}
	}
	else
	{
		unsigned header, version;
		if (gzread(gz, &header, sizeof header) != sizeof version) return level_load_thread_abort(l, "header");
		if (header != 'MCLV') return level_load_thread_abort(l, "invalid header");
		if (gzread(gz, &version, sizeof version) != sizeof version) return level_load_thread_abort(l, "version");

		if (version < 2)
		{
			unsigned x, y, z;
			if (gzread(gz, &x, sizeof x) != sizeof x) return level_load_thread_abort(l, "x");
			if (gzread(gz, &y, sizeof y) != sizeof y) return level_load_thread_abort(l, "y");
			if (gzread(gz, &z, sizeof z) != sizeof z) return level_load_thread_abort(l, "z");
			if (!level_init(l, x, y, z, name, false)) return level_load_thread_abort(l, "level init failed");
		}
		else
		{
			int16_t x, y, z;
			if (gzread(gz, &x, sizeof x) != sizeof x) return level_load_thread_abort(l, "x");
			if (gzread(gz, &y, sizeof y) != sizeof y) return level_load_thread_abort(l, "y");
			if (gzread(gz, &z, sizeof z) != sizeof z) return level_load_thread_abort(l, "z");
			if (!level_init(l, x, y, z, name, false)) return level_load_thread_abort(l, "level init failed");
		}

		if (gzread(gz, &l->spawn, sizeof l->spawn) != sizeof l->spawn) return level_load_thread_abort(l, "spawn");

		int s = sizeof *l->blocks * l->x * l->y * l->z;
		if (gzread(gz, l->blocks, s) != s) return level_load_thread_abort(l, "blocks");

		if (version == 0)
		{
			l->owner = 0;
			l->rankvisit = RANK_GUEST;
			l->rankbuild = RANK_GUEST;
			l->rankown   = RANK_OP;
		}
		else
		{
			if (gzread(gz, &l->owner, sizeof l->owner) != sizeof l->owner) return level_load_thread_abort(l, "owner");
			if (gzread(gz, &l->rankvisit, sizeof l->rankvisit) != sizeof l->rankvisit) return level_load_thread_abort(l, "rankvisit");
			if (gzread(gz, &l->rankbuild, sizeof l->rankbuild) != sizeof l->rankbuild) return level_load_thread_abort(l, "rankbuild");
			if (version < 5)
			{
				l->rankvisit = rank_convert(l->rankvisit);
				l->rankbuild = rank_convert(l->rankbuild);
				l->rankown = RANK_OP;
			}
			else
			{
				if (gzread(gz, &l->rankown, sizeof l->rankown) != sizeof l->rankown) return level_load_thread_abort(l, "rankown");
				if (version < 6)
				{
					l->rankvisit = rank_convert(l->rankvisit);
					l->rankbuild = rank_convert(l->rankbuild);
					l->rankown = rank_convert(l->rankown);
				}
			}

			unsigned n;
			unsigned u;
			if (gzread(gz, &n, sizeof n) != sizeof n) return level_load_thread_abort(l, "uservisit count");
			if (version < 3 && gzread(gz, &u, sizeof n) != sizeof u && u != 0) return level_load_thread_abort(l, "uservisit count (old)");
			for (i = 0; i < (int)n; i++)
			{
				if (gzread(gz, &u, sizeof u) != sizeof u) return level_load_thread_abort(l, "uservisit");
				user_list_add(&l->uservisit, u);
			}

			if (gzread(gz, &n, sizeof n) != sizeof n) return level_load_thread_abort(l, "userbuild count");
			if (version < 3 && gzread(gz, &u, sizeof n) != sizeof u && u != 0) return level_load_thread_abort(l, "userbuild count (old)");
			for (i = 0; i < (int)n; i++)
			{
				if (gzread(gz, &u, sizeof u) != sizeof u) return level_load_thread_abort(l, "userbuild");
				user_list_add(&l->userbuild, u);
			}

			if (version >= 5)
			{
				if (gzread(gz, &n, sizeof n) != sizeof n) return level_load_thread_abort(l, "userown count");
				for (i = 0; i < (int)n; i++)
				{
					if (gzread(gz, &u, sizeof u) != sizeof u) return level_load_thread_abort(l, "userown");
					user_list_add(&l->userown, u);
				}
			}
		}

		if (version >= 4)
		{
			unsigned n;
			if (gzread(gz, &n, sizeof n) != sizeof n) return level_load_thread_abort(l, "level_hooks");

			for (i = 0; i < (int)n; i++)
			{
				gzread(gz, l->level_hook[i].name, sizeof l->level_hook[i].name);
				gzread(gz, &l->level_hook[i].data.size, sizeof l->level_hook[i].data.size);
				if (l->level_hook[i].data.size == 0)
				{
					l->level_hook[i].data.data = NULL;
				}
				else
				{
					l->level_hook[i].data.data = malloc(l->level_hook[i].data.size);
				}
				gzread(gz, l->level_hook[i].data.data, l->level_hook[i].data.size);

				if (*l->level_hook[i].name != '\0')
				{
					level_hook_attach(l, l->level_hook[i].name);
				}
			}
		}

		int count = l->x * l->y * l->z;
		for (i = 0; i < count; i++)
		{
			struct block_t *b = &l->blocks[i];
			if (b->physics) physics_list_add(&l->physics, i);
		}
	}

	gzclose(gz);

	LOG("Level '%s' loaded\n", l->name);

	pthread_mutex_unlock(&l->mutex);

	return NULL;
}

bool level_load(const char *name, struct level_t **levelp)
{
	bool convert = false;
	char filename[64];
	snprintf(filename, sizeof filename, "levels/%s.mcl", name);
	lcase(filename);

	FILE *f = fopen(filename, "rb");
	if (f == NULL)
	{
		snprintf(filename, sizeof filename, "levels/%s.lvl", name);
		lcase(filename);

		f = fopen(filename, "rb");
		if (f == NULL) return false;

		convert = true;
	}
	fclose(f);

	struct level_t *level = malloc(sizeof *level);
	memset(level, 0, sizeof *level);
	pthread_mutex_init(&level->mutex, NULL);
	pthread_mutex_init(&level->inuse_mutex, NULL);
	pthread_mutex_init(&level->hook_mutex, NULL);
	pthread_mutex_init(&level->physics_mutex, NULL);

	level_list_add(&s_levels, level);
	if (levelp != NULL) *levelp = level;

	strncpy(level->name, name, sizeof level->name);
	level->convert = convert;

	pthread_mutex_lock(&level->mutex);
	level_load_queue(level);

	return true;
}

void *level_save_thread(void *arg)
{
	struct level_t *l = arg;

	pthread_mutex_lock(&l->mutex);

	l->changed = false;

	call_level_hook(EVENT_SAVE, l, NULL, NULL);

	char filenametmp[256];
	snprintf(filenametmp, sizeof filenametmp, "levels/%s.mcl.tmp", l->name);
	lcase(filenametmp);

	gzFile gz = gzopen(filenametmp, "wb");
	if (gz == NULL)
	{
		pthread_mutex_unlock(&l->mutex);
		level_inuse(l, false);
		return NULL;
	}

	LOG("Saving level '%s'\n", l->name);

	unsigned header  = 'MCLV';
	unsigned version = 6;
	gzwrite(gz, &header, sizeof header);
	gzwrite(gz, &version, sizeof version);

	gzwrite(gz, &l->x, sizeof l->x);
	gzwrite(gz, &l->y, sizeof l->y);
	gzwrite(gz, &l->z, sizeof l->z);
	gzwrite(gz, &l->spawn, sizeof l->spawn);
	gzwrite(gz, l->blocks, sizeof *l->blocks * l->x * l->y * l->z);

	gzwrite(gz, &l->owner, sizeof l->owner);
	gzwrite(gz, &l->rankvisit, sizeof l->rankvisit);
	gzwrite(gz, &l->rankbuild, sizeof l->rankbuild);
	gzwrite(gz, &l->rankown, sizeof l->rankown);

	unsigned i;
	i = l->uservisit.used;
	gzwrite(gz, &i, sizeof i);
	for (i = 0; i < l->uservisit.used; i++)
	{
		gzwrite(gz, &l->uservisit.items[i], sizeof l->uservisit.items[i]);
	}

	i = l->userbuild.used;
	gzwrite(gz, &i, sizeof i);
	for (i = 0; i < l->userbuild.used; i++)
	{
		gzwrite(gz, &l->userbuild.items[i], sizeof l->userbuild.items[i]);
	}

	i = l->userown.used;
	gzwrite(gz, &i, sizeof i);
	for (i = 0; i < l->userown.used; i++)
	{
		gzwrite(gz, &l->userown.items[i], sizeof l->userown.items[i]);
	}

	i = MAX_HOOKS_PER_LEVEL;
	gzwrite(gz, &i, sizeof i);
	for (i = 0; i < MAX_HOOKS_PER_LEVEL; i++)
	{
		gzwrite(gz, l->level_hook[i].name, sizeof l->level_hook[i].name);
		gzwrite(gz, &l->level_hook[i].data.size, sizeof l->level_hook[i].data.size);
		gzwrite(gz, l->level_hook[i].data.data, l->level_hook[i].data.size);
	}

	gzclose(gz);

	LOG("Level '%s' saved\n", l->name);

	char backup[256];
	snprintf(backup, sizeof backup, "levels/backups/%s-%lld.mcl", l->name, (long long int)time(NULL));
	lcase(backup);

	pthread_mutex_unlock(&l->mutex);

	char filename[256];
	snprintf(filename, sizeof filename, "levels/%s.mcl", l->name);
	lcase(filename);

	rename(filenametmp, filename);

	level_inuse(l, false);

	/* Copy the file to back up */
	int src = open(filename, O_RDONLY);
	int dst = open(backup, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	char buf[1024];
	size_t len;
	while ((len = read(src, buf, sizeof buf)) > 0)
	{
		write(dst, buf, len);
	}

	close(src);
	close(dst);

	LOG("Backed up %s to %s\n", filename, backup);

	return NULL;
}

void level_save(struct level_t *l)
{
	if (!level_inuse(l, true)) return;

	level_save_queue(l);
}

bool level_is_loaded(const char *name)
{
	unsigned i;
	const struct level_t *l;

	for (i = 0; i < s_levels.used; i++)
	{
		l = s_levels.items[i];
		if (l != NULL && strcasecmp(l->name, name) == 0)
		{
			return true;
		}
	}

	return false;
}

bool level_get_by_name(const char *name, struct level_t **level)
{
	unsigned i;
	struct level_t *l;

	for (i = 0; i < s_levels.used; i++)
	{
		l = s_levels.items[i];
		if (l != NULL && strcasecmp(l->name, name) == 0)
		{
			if (level != NULL) *level = l;
			return true;
		}
	}

	/* See if level is saved? */
	if (level_load(name, &l))
	{
		if (level != NULL) *level = l;
		return true;
	}

	return false;
}

void level_save_all(void *arg)
{
	unsigned i;

	for (i = 0; i < s_levels.used; i++)
	{
		if (s_levels.items[i] != NULL && s_levels.items[i]->changed)
		{
			level_save(s_levels.items[i]);
		}
	}
}

static bool level_is_empty(const struct level_t *l)
{
	unsigned i;

	pthread_mutex_lock(&s_client_list_mutex);

	for (i = 0; i < s_clients.used; i++)
	{
		const struct client_t *c = s_clients.items[i];
		if (c->player == NULL) continue;
		if (c->player->level == l || c->player->new_level == l)
		{
			pthread_mutex_unlock(&s_client_list_mutex);
			return false;
		}
	}

	pthread_mutex_unlock(&s_client_list_mutex);
	return true;
}

void level_unload_empty(void *arg)
{
	unsigned i;

	for (i = 0; i < s_levels.used; i++)
	{
		struct level_t *l = s_levels.items[i];

		if (l == NULL || l->changed) continue;

		/* Test if another thread is accessing... */
		if (pthread_mutex_trylock(&l->mutex) != 0) continue;
		pthread_mutex_unlock(&l->mutex);

		if (!level_is_empty(l)) continue;

		pthread_mutex_lock(&l->inuse_mutex);
		if (l->inuse > 0)
		{
			LOG("Level %s use count %d\n", l->name, l->inuse);
			pthread_mutex_unlock(&l->inuse_mutex);
			continue;
		}
		l->inuse = -1;
		pthread_mutex_unlock(&l->inuse_mutex);

		level_unload(l);
		//level_list_del(&s_levels, l);

		s_levels.items[i] = NULL;
	}
}

bool level_get_xyz(const struct level_t *level, unsigned index, int16_t *x, int16_t *y, int16_t *z)
{
//	if (index >= level->x * level->y * level->z) return false;

	if (x != NULL) *x = index % level->x;
	if (y != NULL) *y = index / level->x / level->z;
	if (z != NULL) *z = (index / level->x) % level->z;

	return true;
}

void level_copy(struct level_t *src, struct level_t *dst)
{
	if (!level_inuse(src, true)) return;
	if (!level_inuse(dst, true))
	{
		level_inuse(src, false);
		return;
	}

	struct cuboid_t c;

	c.sx = c.ex = c.cx = 0;
	c.sy = c.ey = c.cy = 0;
	c.sz = c.ez = c.cz = 0;
	c.level = dst;
	c.srclevel = src;
	c.count = 0;
	c.old_type = BLOCK_INVALID;
	c.new_type = BLOCK_INVALID;
	c.owner = 0;
	c.owner_is_op = true;
	c.fixed = false;
	c.undo = false;
	c.client = NULL;

	cuboid_list_add(&s_cuboids, c);

	dst->no_changes = 1;
}

void level_cuboid(struct level_t *level, unsigned start, unsigned end, enum blocktype_t old_type, enum blocktype_t new_type, const struct player_t *p)
{
	struct cuboid_t c;

	if (!level_get_xyz(level, start, &c.sx, &c.sy, &c.sz)) return;
	if (!level_get_xyz(level, end, &c.ex, &c.ey, &c.ez)) return;

	if (!level_inuse(level, true)) return;

	int16_t t;
	if (c.ex < c.sx) { t = c.sx; c.sx = c.ex; c.ex = t; }
	if (c.ey < c.sy) { t = c.sy; c.sy = c.ey; c.ey = t; }
	if (c.ez < c.sz) { t = c.sz; c.sz = c.ez; c.ez = t; }

	c.cx = c.sx;
	c.cy = c.ey;
	c.cz = c.sz;
	c.level = level;
	c.srclevel = NULL;
	c.count = 0;
	c.old_type = old_type;
	c.new_type = new_type;
	c.owner = HasBit(p->flags, FLAG_DISOWN) ? 0 : p->globalid;
	c.owner_is_op = (p->rank >= RANK_OP) || level_user_can_own(level, p);
	c.fixed = HasBit(p->flags, FLAG_PLACE_FIXED);
	c.undo = false;
	c.client = p->client;

	cuboid_list_add(&s_cuboids, c);
}

void level_user_undo(struct level_t *level, unsigned globalid, struct client_t *client)
{
	if (!level_inuse(level, true)) return;

	struct cuboid_t c;

	c.sx = 0;
	c.sy = 0;
	c.sz = 0;
	c.ex = level->x - 1;
	c.ey = level->y - 1;
	c.ez = level->z - 1;
	c.cx = c.sx;
	c.cy = c.ey;
	c.cz = c.sz;
	c.level = level;
	c.srclevel = NULL;
	c.count = 0;
	c.old_type = BLOCK_INVALID;
	c.new_type = AIR;
	c.owner = globalid;
	c.owner_is_op = false;
	c.fixed = false;
	c.undo = true;
	c.client = client;

	cuboid_list_add(&s_cuboids, c);
}

void level_change_block(struct level_t *level, struct client_t *client, int16_t x, int16_t y, int16_t z, uint8_t m, uint8_t t, bool click)
{
	if (client->player == NULL || client->player->rank == RANK_BANNED)
	{
		/* Ignore banned players :D */
		return;
	}

	if (client->waiting_for_level)
	{
		client_notify(client, "Change ignored whilst waiting for level change!");
		return;
	}

	if (x < 0 || y < 0 || z < 0 || x >= level->x || y >= level->y || z >= level->z)
	{
		return;
	}

	unsigned index = level_get_index(level, x, y, z);
	struct block_t *b = &level->blocks[index];
	enum blocktype_t bt = b->type;
	bool ingame = HasBit(client->player->flags, FLAG_GAMES);

	if (level->no_changes)
	{
		client_add_packet(client, packet_send_set_block(x, y, z, convert(level, index, b)));
		return;
	}

	if (click)
	{
		if (t >= BLOCK_END)
		{
			net_close(client, "Anti-grief: tried to place invalid block");
			return;
		}

		switch (t)
		{
			case AIR:
			case GRASS:
			case ADMINIUM:
			case WATER:
			case WATERSTILL:
			case LAVA:
			case LAVASTILL:
			case STAIRCASEFULL:
				net_close(client, "Anti-grief: tried to place unplaceable block");
				return;
		}

		/* Check block distcance */
		int distance = abs(client->player->pos.x / 32 - x);
		distance += abs(client->player->pos.y / 32 - y);
		distance += abs(client->player->pos.z / 32 - z);

		if (distance > 12 && !ingame && !client->player->teleport)
		{
			net_close(client, "Anti-grief: built too far away");
			return;
		}
		if (distance > 10)
		{
			client_notify(client, "You cannot build that far away");
			client_add_packet(client, packet_send_set_block(x, y, z, convert(level, index, b)));
			return;
		}
	}

	enum blocktype_t nt = (click && !ingame) ? client->player->bindings[t] : t;

	if ((click && client->player->mode == MODE_INFO) || nt == -1)
	{
		char buf[64];
		snprintf(buf, sizeof buf, "%s%s at %dx%dx%d placed by %s",
				 b->fixed ? "fixed " : "",
				 blocktype_get_name(bt), x, y, z,
				 b->owner == 0 ? "none" : playerdb_get_username(b->owner)
		);
		client_notify(client, buf);

		snprintf(buf, sizeof buf, "Physics: %s  Data 0x%04X", b->physics ? "yes" : "no", b->data);
		client_notify(client, buf);

		client_add_packet(client, packet_send_set_block(x, y, z, convert(level, index, b)));
		return;
	}

	bool can_build = level_user_can_build(level, client->player);

	if (click && can_build && !ingame)
	{
		if (client->player->mode == MODE_CUBOID)
		{
			client_add_packet(client, packet_send_set_block(x, y, z, convert(level, index, b)));

			if (client->player->cuboid_start == UINT_MAX)
			{
				client->player->cuboid_start = index;
				client_notify(client, "Cuboid start placed");
				return;
			}

			nt = (client->player->cuboid_type == BLOCK_INVALID) ? nt : client->player->cuboid_type;
			if (client->player->globalid != level->owner && client->player->rank < blocktype_min_rank(nt))
			{
				char buf[128];
				snprintf(buf, sizeof buf, "You do not have permission to place %s", blocktype_get_name(nt));
				client_notify(client, buf);
				client->player->mode = MODE_NORMAL;
				return;
			}

			client_notify(client, "Cuboid end placed");
			level_cuboid(level, client->player->cuboid_start, index, -1, nt, client->player);
			client->player->mode = MODE_NORMAL;
			return;
		}
		else if (client->player->mode == MODE_REPLACE)
		{
			client_add_packet(client, packet_send_set_block(x, y, z, convert(level, index, b)));

			if (client->player->cuboid_start == UINT_MAX)
			{
				client->player->cuboid_start = index;
				client_notify(client, "Replace start placed");
				return;
			}

			nt = (client->player->cuboid_type == BLOCK_INVALID) ? nt : client->player->cuboid_type;
			if (client->player->globalid != level->owner && client->player->rank < blocktype_min_rank(nt))
			{
				char buf[128];
				snprintf(buf, sizeof buf, "You do not have permission to place %s", blocktype_get_name(nt));
				client_notify(client, buf);
				client->player->mode = MODE_NORMAL;
				return;
			}

			client_notify(client, "Replace end placed");
			level_cuboid(level, client->player->cuboid_start, index, client->player->replace_type, nt, client->player);
			client->player->mode = MODE_NORMAL;
			return;
		}
		else if (client->player->mode == MODE_REMOVE_PILLAR)
		{
			unsigned indexs = index, indexe = index;
			int sy, ey;
			/* Search up */
			for (ey = y; ey < level->y; ey++)
			{
				index = level_get_index(level, x, ey, z);
				struct block_t *b2 = &level->blocks[index];
				enum blocktype_t bt2 = b2->type;
				if (bt != bt2) break;
				indexe = index;
			}
			/* Search down */
			for (sy = y; sy > 0; sy--)
			{
				index = level_get_index(level, x, sy, z);
				struct block_t *b2 = &level->blocks[index];
				enum blocktype_t bt2 = b2->type;
				if (bt != bt2) break;
				indexs = index;
			}
			level_cuboid(level, indexs, indexe, BLOCK_INVALID, AIR, client->player);
			return;
		}
	}

	if (click && !ingame)
	{
		if (client->player->mode == MODE_PLACE_SOLID) nt = ADMINIUM;
		else if (client->player->mode == MODE_PLACE_WATER) nt = WATERSTILL;
		else if (client->player->mode == MODE_PLACE_LAVA) nt = LAVASTILL;
		else if (client->player->mode == MODE_PLACE_ACTIVE_WATER)
		{
			client->player->mode = MODE_NORMAL;
			nt = WATER;
			client_notify(client, "Active water " TAG_GREEN "off");
		}
		else if (client->player->mode == MODE_PLACE_ACTIVE_LAVA)
		{
			client->player->mode = MODE_NORMAL;
			nt = LAVA;
			client_notify(client, "Active lava " TAG_GREEN "off");
		}
	}

	/* Client thinks it has changed to air */
	if (m == 0) t = AIR;
	if (click && (!HasBit(client->player->flags, FLAG_PAINT) || ingame))
	{
		if (m == 0) {
			int r = trigger(level, index, b, client);
			if (r != TRIG_NONE)
			{
				if (r == TRIG_FILL) client_add_packet(client, packet_send_set_block(x, y, z, convert(level, index, b)));

				//LOG("Triggered!");
				return;
			}

			nt = AIR;
		}

		if (m == 1 && bt != AIR && bt != WATER && bt != LAVA && bt != WATERSTILL && bt != LAVASTILL)
		{
			client_notify(client, "Active physics block cannot be changed");
			client_add_packet(client, packet_send_set_block(x, y, z, convert(level, index, b)));
			return;
		}
	}

	if (!can_build)
	{
		client_notify(client, "You can't build on this level");
		client_add_packet(client, packet_send_set_block(x, y, z, convert(level, index, b)));
		return;
	}

	if (client->player->globalid != level->owner || ingame)
	{
		/* Not level owner, so check block permissions */

		if ((ingame || client->player->rank < RANK_OP) && (bt == ADMINIUM || b->fixed))
			// || (b->owner != 0 && b->owner != client->player->globalid)))
		{
			client_notify(client, "Block cannot be changed");
			client_add_packet(client, packet_send_set_block(x, y, z, convert(level, index, b)));
			return;
		}

		if (client->player->rank < blocktype_min_rank(nt))
		{
			LOG("[%s] %s tried to place %s (%d)\n", level->name, client->player->username, blocktype_get_name(nt), nt);
			if (client->player->rank <= RANK_GUEST)
			{
				switch (t)
				{
					case ADMINIUM:
					case WATER:
					case WATERSTILL:
					case LAVA:
					case LAVASTILL:
						net_close(client, "Anti-grief: tried to place special block");
						return;
				}
			}
			client_add_packet(client, packet_send_set_block(x, y, z, convert(level, index, b)));
			return;
		}

		/* Air with owner can be replaced */
		if ((b->owner != 0 && b->owner != client->player->globalid && b->type != AIR) && !level_user_can_own(level, client->player))
		{
			client_notify(client, "Block cannot be changed");
			client_add_packet(client, packet_send_set_block(x, y, z, convert(level, index, b)));
			return;
		}
	}

	struct block_event be;
	be.x = x; be.y = y; be.z = z;
	be.bt = bt; be.nt = nt; be.data = 0;

	call_level_hook(EVENT_BLOCK, level, client, &be);

	if (bt != be.nt)
	{
		if (level->undo == NULL)
		{
			level->undo = undodb_init(level->name);
		}
		undodb_log(level->undo, client->player->globalid, x, y, z, b->type, b->data, be.nt);
//		player_undo_log(client->player, index);

		delete(level, index, b);

		bool oldphysics = b->physics;

		b->type = be.nt;
		b->data = be.data;
		b->fixed = ingame ? false : HasBit(client->player->flags, FLAG_PLACE_FIXED);
		b->owner = !ingame && HasBit(client->player->flags, FLAG_DISOWN) ? 0 : client->player->globalid;
		b->physics = blocktype_has_physics(nt);

		if (oldphysics != b->physics)
		{
			physics_list_update(level, index, b->physics);
		}

		level->changed = true;

		enum blocktype_t pt = convert(level, index, b);

		unsigned i;
		for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
		{
			struct client_t *c = level->clients[i];
			if (c == NULL || c->sending_level) continue;

			if (client != c || pt != t || !click)
			{
				client_add_packet(c, packet_send_set_block(x, y, z, pt));
			}
		}
	}
	else
	{
		enum blocktype_t pt = convert(level, index, b);

		if (pt != t)
		{
			/* Block hasn't changed but client thinks it has? */
			client_add_packet(client, packet_send_set_block(x, y, z, pt));
		}
	}
}

void level_change_block_force(struct level_t *level, struct block_t *block, unsigned index)
{
	unsigned i;
	struct block_t *b = &level->blocks[index];
	*b = *block;
	level->changed = true;

	int16_t x, y, z;
	if (!level_get_xyz(level, index, &x, &y, &z)) return;

	for (i = 0; i < s_clients.used; i++)
	{
		struct client_t *c = s_clients.items[i];
		if (c->player == NULL) continue;
		if (c->player->level == level)
		{
			client_add_packet(c, packet_send_set_block(x, y, z, convert(level, index, b)));
		}
	}
}

static void level_run_physics(struct level_t *level, bool can_init, bool limit)
{
	/* Don't run physics if updates are being done */
	if (level->physics_done == 1) return;

	if (level->physics_iter == 0)
	{
		if (!can_init) return;

		//LOG("Starting physics run with %lu blocks\n", level->physics.used)
		level->physics_runtime = 0;

		pthread_mutex_lock(&level->physics_mutex);

		/* Swap physics list */
		unsigned *p = level->physics2.items;
		level->physics2.items = level->physics.items;
		level->physics.items = p;

		size_t u = level->physics2.used;
		level->physics2.used = level->physics.used;
		level->physics.used = u;

		u = level->physics2.size;
		level->physics2.size = level->physics.size;
		level->physics.size = u;

		pthread_mutex_unlock(&level->physics_mutex);
	}

	//LOG("Done %d out of %lu\n", level->physics_iter, level->physics2.used);

	int s = gettime();

	//LOG("%lu physics blocks, iterator at %d\n", level->physics.used, level->physics_iter);
	for (; level->physics_iter < level->physics2.used; level->physics_iter++)
	{
		unsigned index = level->physics2.items[level->physics_iter];
		struct block_t *b = &level->blocks[index];

		physics(level, index, b);

		if (limit && gettime() - s > 40) {
			level->physics_runtime += gettime() - s;
			return;
		}
	}

	/* Log if physics took too long */
	if (level->physics_runtime > 40)
	{
		LOG("Physics on %s ran in %dms (%zu blocks)\n", level->name, level->physics_runtime, level->physics2.used);
	}

	level->physics_runtime_last = level->physics_runtime;
	level->physics_count_last = level->physics2.used;

	level->physics_done = 1;
	level->physics2.used = 0;
}

static void level_run_updates(struct level_t *level, bool can_init, bool limit)
{
	/* Don't run updates until physics are complete */
	if (level->physics_done == 0) return;
	if (level->physics_pause) return;

	//LOG("%lu block updates, iterator at %d\n", level->updates.used, level->updates_iter);

	int s = gettime();

	if (level->updates_iter == 0)
	{
		if (!can_init) return;

		level->updates_runtime = 0;
	}

	//LOG("Done %d out of %lu\n", level->updates_iter, level->updates.used);

	if (limit)
	{
		int n = g_server.cuboid_max;
		for (; level->updates_iter < level->updates.used && n > 0; level->updates_iter++, n--)
		{
			struct block_update_t *bu = &level->updates.items[level->updates_iter];

			int16_t x, y, z;
			level_get_xyz(level, bu->index, &x, &y, &z);

			unsigned j;
			for (j = 0; j < MAX_CLIENTS_PER_LEVEL; j++)
			{
				struct client_t *c = level->clients[j];
				if (c == NULL || c->player == NULL) continue;
				if (!c->waiting_for_level && !c->sending_level)
				{
					client_add_packet(c, packet_send_set_block(x, y, z, bu->newtype));
				}
			}
		}

		level->updates_runtime += gettime() - s;

		/* Did updates complete? */
		if (level->updates_iter < level->updates.used) return;

		if (level->updates_runtime > 40)
		{
			LOG("Updates on %s ran in %dms (%zu blocks)\n", level->name, level->updates_runtime, level->updates.used);
		}

		level->updates_runtime_last = level->updates_runtime;
		level->updates_count_last = level->updates.used;
	}

	//LOG("Hmm (%lu / %lu blocks)\n", level->physics.used, level->physics2.used);

	level->updates.used = 0;
	level->updates_iter = 0;
	level->physics_iter = 0;

	level->physics_done = 0;
}

void level_addupdate(struct level_t *level, unsigned index, enum blocktype_t newtype, uint16_t newdata)
{
	struct block_t *b = &level->blocks[index];

	enum blocktype_t pt1 = convert(level, index, b);

	b->type = newtype;
	b->data = newdata;
	b->physics = blocktype_has_physics(b->type);

	if (b->physics) physics_list_update(level, index, b->physics);

	enum blocktype_t pt2 = convert(level, index, b);

	if (pt1 != pt2 && !level->instant)
	{
		struct block_update_t bu;
		bu.index = index;
		bu.newtype = pt2;

		block_update_list_add(&level->updates, bu);
	}
}

void level_addupdate_with_owner(struct level_t *level, unsigned index, enum blocktype_t newtype, uint16_t newdata, unsigned owner)
{
	struct block_t *b = &level->blocks[index];

	enum blocktype_t pt1 = convert(level, index, b);

	b->type = newtype;
	b->data = newdata;
	b->owner = owner;
	b->physics = blocktype_has_physics(b->type);

	if (b->physics) physics_list_update(level, index, b->physics);

	enum blocktype_t pt2 = convert(level, index, b);

	if (pt1 != pt2 && !level->instant)
	{
		struct block_update_t bu;
		bu.index = index;
		bu.newtype = pt2;

		block_update_list_add(&level->updates, bu);
	}
}

void level_prerun(struct level_t *l)
{
	int n;

	for (n = 0; n < 4; n++)
	{
		level_run_physics(l, true, false);
		level_run_updates(l, true, false);
	}
}

void level_process_physics(bool can_init)
{
	unsigned i;
	for (i = 0; i < s_levels.used; i++)
	{
		struct level_t *level = s_levels.items[i];
		if (level == NULL) continue;

		if (!level_inuse(level, true)) continue;

		/* Don't run physics for empty levels, else it will never unload */
		if (level_is_empty(level)) {
			level_inuse(level, false);
			continue;
		}

		/* Test if another thread is accessing... */
		if (pthread_mutex_trylock(&level->mutex) != 0) {
			level_inuse(level, false);
			continue;
		}
		pthread_mutex_unlock(&level->mutex);

		level_run_physics(level, can_init, true);

		call_level_hook(EVENT_TICK, level, NULL, NULL);
		level_inuse(level, false);
	}
}

void level_process_updates(bool can_init)
{
	unsigned i;
	for (i = 0; i < s_levels.used; i++)
	{
		struct level_t *level = s_levels.items[i];
		if (level == NULL) continue;

		if (!level_inuse(level, true)) continue;

		/* Don't run physics for empty levels, else it will never unload */
		if (level_is_empty(level)) {
			level_inuse(level, false);
			continue;
		}

		/* Test if another thread is accessing... */
		if (pthread_mutex_trylock(&level->mutex) != 0) {
			level_inuse(level, false);
			continue;
		}
		pthread_mutex_unlock(&level->mutex);

		level_run_updates(level, can_init, !level->instant);
		level_inuse(level, false);
	}
}


struct level_hook_t
{
	char name[16];
	level_hook_func_t level_hook_func;
};

static inline bool level_hook_compare(struct level_hook_t *a, struct level_hook_t *b)
{
	return strcmp(a->name, b->name) == 0;
}

LIST(level_hook, struct level_hook_t, level_hook_compare)

static struct level_hook_list_t s_level_hooks;

static struct level_hook_t *level_hook_get_by_name(const char *name)
{
	unsigned i;
	for (i = 0; i < s_level_hooks.used; i++)
	{
		struct level_hook_t *lh = &s_level_hooks.items[i];
		if (strcasecmp(lh->name, name) == 0) return lh;
	}

	return NULL;
}

void register_level_hook_func(const char *name, level_hook_func_t level_hook_func)
{
	if (level_hook_get_by_name(name) != NULL)
	{
		LOG("Level hook %s already registered\n", name);
		return;
	}

	struct level_hook_t lh;
	strncpy(lh.name, name, sizeof lh.name);
	lh.level_hook_func = level_hook_func;

	level_hook_list_add(&s_level_hooks, lh);

	LOG("Registered level hook %s\n", name);

	unsigned i, j;
	for (i = 0; i < s_levels.used; i++)
	{
		struct level_t *l = s_levels.items[i];
		if (l == NULL) continue;
		for (j = 0; j < MAX_HOOKS_PER_LEVEL; j++)
		{
			if (l->level_hook[j].func == NULL && strcasecmp(l->level_hook[j].name, name) == 0)
			{
				level_hook_attach(l, name);
			}
		}
	}
}

void deregister_level_hook_func(const char *name)
{
	struct level_hook_t lh;
	strncpy(lh.name, name, sizeof lh.name);

	unsigned i;
	for (i = 0; i < s_levels.used; i++)
	{
		struct level_t *l = s_levels.items[i];
		if (l == NULL) continue;

		level_hook_detach(l, name);
	}

	level_hook_list_del_item(&s_level_hooks, lh);
	LOG("Deregistered level hook %s\n", name);
}

void level_hooks_deinit(void)
{
	while (s_level_hooks.used > 0)
	{
		LOG("Level hook %s not deregistered\n", s_level_hooks.items[0].name);
		level_hook_list_del_index(&s_level_hooks, 0);
	}

	level_hook_list_free(&s_level_hooks);
}

bool level_hook_attach(struct level_t *l, const char *name)
{
	unsigned i, j, n = -1, m = -1;
	for (i = 0; i < s_level_hooks.used; i++)
	{
		if (strcasecmp(s_level_hooks.items[i].name, name) == 0)
		{
			m = i;
			break;
		}
	}

	if (m == -1)
	{
		LOG("Hook not found\n");
		return false;
	}

	for (j = 0; j < MAX_HOOKS_PER_LEVEL; j++)
	{
		if (*l->level_hook[j].name == '\0' && n == -1)
		{
			n = j;
		}

		if (strcasecmp(l->level_hook[j].name, s_level_hooks.items[i].name) == 0)
		{
			if (l->level_hook[j].func != NULL)
			{
				LOG("Hook already attached\n");
				return false;
			}
			n = j;
			break;
		}
	}

	if (n == -1)
	{
		LOG("Slot not found\n");
		return false;
	}

	pthread_mutex_lock(&l->hook_mutex);

	strcpy(l->level_hook[n].name, s_level_hooks.items[m].name);
	l->level_hook[n].func = s_level_hooks.items[m].level_hook_func;

	char buf[128];
	snprintf(buf, sizeof buf, TAG_YELLOW "Attached level hook %s (%d)", l->level_hook[n].name, n + 1);
	level_notify_all(l, buf);

	l->level_hook[n].func(EVENT_INIT, l, NULL, NULL, &l->level_hook[n].data);

	pthread_mutex_unlock(&l->hook_mutex);

	return true;
}

bool level_hook_detach(struct level_t *l, const char *name)
{
	unsigned i;
	for (i = 0; i < MAX_HOOKS_PER_LEVEL; i++)
	{
		if (strcasecmp(l->level_hook[i].name, name) == 0 && l->level_hook[i].func != NULL)
		{
			pthread_mutex_lock(&l->hook_mutex);

			l->level_hook[i].func(EVENT_DEINIT, l, NULL, NULL, &l->level_hook[i].data);

			char buf[128];
			snprintf(buf, sizeof buf, TAG_YELLOW "Detached level hook %s (%d)", l->level_hook[i].name, i + 1);
			level_notify_all(l, buf);

			l->level_hook[i].func = NULL;

			pthread_mutex_unlock(&l->hook_mutex);

			return true;
		}
	}

	return false;
}

bool level_hook_delete(struct level_t *l, const char *name)
{
	unsigned i;
	for (i = 0; i < MAX_HOOKS_PER_LEVEL; i++)
	{
		if (strcasecmp(l->level_hook[i].name, name) == 0)
		{
			pthread_mutex_lock(&l->hook_mutex);

			if (l->level_hook[i].func != NULL)
			{
				l->level_hook[i].func(EVENT_DEINIT, l, NULL, NULL, &l->level_hook[i].data);

				char buf[128];
				snprintf(buf, sizeof buf, TAG_YELLOW "Deleted level hook %s (%d)", l->level_hook[i].name, i + 1);
				level_notify_all(l, buf);

				l->level_hook[i].func = NULL;
			}

			*l->level_hook[i].name = '\0';
			free(l->level_hook[i].data.data);
			l->level_hook[i].data.data = NULL;
			l->level_hook[i].data.size = 0;

			pthread_mutex_unlock(&l->hook_mutex);

			return true;
		}
	}

	return false;
}

bool call_level_hook(int hook, struct level_t *l, struct client_t *c, void *data)
{
	if (l == NULL) return false;

	pthread_mutex_lock(&l->hook_mutex);

	unsigned i;
	for (i = 0; i < MAX_HOOKS_PER_LEVEL; i++)
	{
		if (l->level_hook[i].func == NULL) continue;
		if (l->level_hook[i].func(hook, l, c, data, &l->level_hook[i].data))
		{
			pthread_mutex_unlock(&l->hook_mutex);
			return true;
		}
	}

	pthread_mutex_unlock(&l->hook_mutex);

	return false;
}

bool level_inuse(struct level_t *level, bool inuse)
{
	pthread_mutex_lock(&level->inuse_mutex);

	if (level->inuse < 0) {
		pthread_mutex_unlock(&level->inuse_mutex);
		return false;
	}

	level->inuse += inuse ? 1 : -1;

//	LOG("Level %s in use %d (%s)\n", level->name, level->inuse, inuse ? "add" : "del");

	pthread_mutex_unlock(&level->inuse_mutex);

	return true;
}

static pthread_t s_physics_thread;
static bool s_physics_exit;

void *physics_thread(void *arg)
{
	pid_t tid = (pid_t)syscall(SYS_gettid);

	LOG("Physics thread (%u) initialised\n", tid);

	nice(10);

	static const unsigned TICK_INTERVAL = 40;
	unsigned cur_ticks = gettime();
	unsigned next_tick = cur_ticks + TICK_INTERVAL;
	int i = 0;

	while (!s_physics_exit)
	{
		unsigned prev_cur_ticks = cur_ticks;
		cur_ticks = gettime();
		if (cur_ticks >= next_tick || cur_ticks < prev_cur_ticks)
		{
			next_tick = cur_ticks + TICK_INTERVAL;
			i = (i + 1) % 2;

			cuboid_process();
			level_process_physics(i);
			level_process_updates(true);
		}
		usleep(g_server.physics_usleep);
	}

	LOG("Physics thread (%u) deinitialised\n", tid);

	return NULL;
}

void physics_init(void)
{
	s_physics_exit = false;
	pthread_create(&s_physics_thread, NULL, &physics_thread, NULL);
}

void physics_deinit(void)
{
	s_physics_exit = true;
	pthread_join(s_physics_thread, NULL);
}

void physics_list_update(struct level_t *level, unsigned index, int state)
{
	pthread_mutex_lock(&level->physics_mutex);
	if (state)
	{
		physics_list_add(&level->physics, index);
		if (level->physics.used > level->x * level->y * level->z)
		{
			LOG("ERROR: physics list on %s larger than level size\n", level->name);
		}
	}
	else
	{
		physics_list_del_item(&level->physics, index);
	}
	pthread_mutex_unlock(&level->physics_mutex);
}
