#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <zlib.h>
#include <pthread.h>
#include <math.h>
#include "filter.h"
#include "level.h"
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
		LOG("level_init: allocation of %lu bytes failed", x * y * z * sizeof *level->blocks);
		return false;
	}

	physics_list_init(&level->physics);

	return true;
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

bool level_send(struct client_t *c)
{
	struct level_t *oldlevel = c->player->level;
	struct level_t *newlevel = c->player->new_level;

	if (newlevel == NULL)
	{
		LOG("Tried moving to NULL level for %s\n", c->player->username);
		c->waiting_for_level = false;
		return false;
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
		client_notify(c, "You do not have sufficient permission to join level");
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
			snprintf(buf, sizeof buf, TAG_WHITE "=%s" TAG_YELLOW " moved to '%s'", c->player->colourusername, newlevel->name);
			call_hook(HOOK_CHAT, buf);
			net_notify_all(buf);

			/* Despawn this user for all users */
			if (!c->hidden) client_send_despawn(c->player->client, false);
			oldlevel->clients[c->player->levelid] = NULL;
		}

		/* Despawn users for this user */
		for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
		{
			if (oldlevel->clients[i] != NULL && !oldlevel->clients[i]->hidden)
			{
				client_add_packet(c, packet_send_despawn_player(i));
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
	}
	client_add_packet(c, packet_send_spawn_player(0xFF, c->player->colourusername, &c->player->pos));

	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		if (newlevel->clients[i] != NULL && newlevel->clients[i] != c && !newlevel->clients[i]->hidden)
		{
			client_add_packet(c, packet_send_spawn_player(i, newlevel->clients[i]->player->colourusername, &newlevel->clients[i]->player->pos));
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

	return true;
}

void *level_gen_thread(void *arg)
{
	int i;
	struct level_t *level = arg;
	struct block_t block;
	int x, y, z;
	int mx = level->x;
	int my = level->y;
	int mz = level->z;
	char buf[64];

	memset(&block, 0, sizeof block);

	memset(level->blocks, 0, sizeof *level->blocks * mx * my * mz);

	if (level->type == 0 || level->type == 1)
	{
		for (z = 0; z < mz; z++)
		{
			for (x = 0; x < mx; x++)
			{
				int h = my / 2 + 1;

				for (y = 0; y < h; y++)
				{
					block.type = (y < h - 5) ? ROCK : (y < h - 1) ? DIRT : GRASS;
					if (level->type == 1 && y == h - 2) block.type = ADMINIUM;
					level_set_block(level, &block, level_get_index(level, x, y, z));
				}
			}
		}
	}
	else
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
		faultgen_create(fg);
		filter_process(ft, faultgen_map(fg));
		faultgen_deinit(fg);
		const float *hm1 = filter_map(ft);


		struct perlin_t *pp = perlin_init(mx, mz, 0.250 * level->type, 6);
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

//		level_gen_heightmap(hm, mx, mz, level->type - 2);
		//level_smooth_slopes(hm, mx, mz, my);

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
				int h = (hm1[x + z * dmx] - avg * 0.5) * level->height_range + my / 2;
				float hm2p = hm2[x + z * dmx];
				int rh = 5;
				if (hm2p < 0.25) rh = hm2p * 20 - 2;

				for (y = 0; y < h && y < my; y++)
				{
					block.type = (y < h - rh) ? ROCK : (y < h - 1) ? DIRT : (y <= level->sea_height) ? SAND : GRASS;
					level_set_block(level, &block, level_get_index(level, x, y, z));
				}

				block.type = WATER;
				for (; y < level->sea_height && y < my; y++)
				{
					level_set_block(level, &block, level_get_index(level, x, y, z));
				}
			}
		}

//		block.type = AIR;
/*
		for (i = 0; i < 16; i++)
		{
			level_gen_heightmap(cmh, mx, mz, level->type - 2);
			level_gen_heightmap(cmd, mx, mz, level->type - 2);

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

	//level_prerun(level);

	LOG("levelgen: %llu physics blocks remaining\n", (long long unsigned)level->physics.used);

	LOG("levelgen: complete\n");

	level->changed = true;

	pthread_mutex_unlock(&level->mutex);

	snprintf(buf, sizeof buf, "Created level '%s'", level->name);
	net_notify_all(buf);

	//LOG(buf);

	return NULL;

level_error:
	level->changed = true;
	pthread_mutex_unlock(&level->mutex);

	snprintf(buf, sizeof buf, TAG_YELLOW "Level generation for %s failed", level->name);
	net_notify_all(buf);

	return NULL;
}

void level_gen(struct level_t *level, int type, int height_range, int sea_height)
{
	level->type = type;
	level->height_range = height_range;
	level->sea_height = sea_height;

	pthread_mutex_lock(&level->mutex);
	if (level->thread_valid)
	{
		pthread_join(level->thread, NULL);
	}
	int r = pthread_create(&level->thread, NULL, &level_gen_thread, level);
	level->thread_valid = (r == 0);
	if (r != 0)
	{
		LOG("Unable to create thread for level generation, server may pause\n");
		level_gen_thread(level);
	}
}

void level_unload(struct level_t *level)
{
	LOG("Level '%s' unloaded\n", level->name);

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

	free(level->level_hook_data.data);
	free(level->blocks);
	free(level);
}

static void *level_load_thread_abort(struct level_t *level, const char *reason)
{
	LOG("Unable to load level %s: %s\n", level->name, reason);

	unsigned i;
	for (i = 0; i < s_clients.used; i++)
	{
		struct client_t *c = s_clients.items[i];
		LOG("client %u: player %p\n", i, c->player);
		if (c->player == NULL) continue;
		LOG("level %p, new_level %p\n", level, c->player->new_level);
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

static void *level_load_thread(void *arg)
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
		}
		else
		{
			l->rankvisit = RANK_GUEST;
			l->rankbuild = RANK_GUEST;
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
		}
		else
		{
			if (gzread(gz, &l->owner, sizeof l->owner) != sizeof l->owner) return level_load_thread_abort(l, "owner");
			if (gzread(gz, &l->rankvisit, sizeof l->rankvisit) != sizeof l->rankvisit) return level_load_thread_abort(l, "rankvisit");
			if (gzread(gz, &l->rankbuild, sizeof l->rankbuild) != sizeof l->rankbuild) return level_load_thread_abort(l, "rankbuild");

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
		}

		if (version == 4)
		{
			unsigned u;
			if (gzread(gz, &u, sizeof u) != sizeof u) return level_load_thread_abort(l, "level_hooks");

			gzread(gz, l->level_hook_name, sizeof l->level_hook_name);
			gzread(gz, &l->level_hook_data.size, sizeof l->level_hook_data.size);
			l->level_hook_data.data = malloc(l->level_hook_data.size);
			gzread(gz, l->level_hook_data.data, l->level_hook_data.size);

			level_hook_attach(l, l->level_hook_name);
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

	level_list_add(&s_levels, level);
	if (levelp != NULL) *levelp = level;

	strncpy(level->name, name, sizeof level->name);
	level->convert = convert;

	pthread_mutex_lock(&level->mutex);
	if (level->thread_valid)
	{
		pthread_join(level->thread, NULL);
	}
	int r = pthread_create(&level->thread, NULL, &level_load_thread, level);
	level->thread_valid = (r == 0);
	if (r != 0)
	{
		LOG("Unable to create thread for level loading, server may pause\n");
		level_load_thread(level);
	}

	return true;
}

void *level_save_thread(void *arg)
{
	struct level_t *l = arg;

	l->changed = false;

	char filename[256];
	snprintf(filename, sizeof filename, "levels/%s.mcl", l->name);
	lcase(filename);

	gzFile gz = gzopen(filename, "wb");
	if (gz == NULL)
	{
		pthread_mutex_unlock(&l->mutex);
		return NULL;
	}

	unsigned header  = 'MCLV';
	unsigned version = 4;
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

	if (*l->level_hook_name == '\0')
	{
		i = 0;
		gzwrite(gz, &i, sizeof i);
	}
	else
	{
		i = 1;
		gzwrite(gz, &i, sizeof i);
		gzwrite(gz, l->level_hook_name, sizeof l->level_hook_name);
		gzwrite(gz, &l->level_hook_data.size, sizeof l->level_hook_data.size);
		gzwrite(gz, l->level_hook_data.data, l->level_hook_data.size);
	}

	gzclose(gz);

	LOG("Level '%s' saved\n", l->name);

	char backup[256];
	snprintf(backup, sizeof backup, "levels/backups/%s-%lld.mcl", l->name, (long long int)time(NULL));
	lcase(backup);

	pthread_mutex_unlock(&l->mutex);

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
	if (pthread_mutex_trylock(&l->mutex) != 0) return;
	if (l->thread_valid)
	{
		pthread_join(l->thread, NULL);
	}
	int r = pthread_create(&l->thread, NULL, &level_save_thread, l);
	l->thread_valid = (r == 0);
	if (r != 0)
	{
		LOG("Unable to create thread for level saving, server may pause\n");
		level_save_thread(l);
	}
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
	for (i = 0; i < s_clients.used; i++)
	{
		const struct client_t *c = s_clients.items[i];
		if (c->player == NULL) continue;
		if (c->player->level == l || c->player->new_level == l) return false;
	}

	return true;
}

void level_unload_empty(void *arg)
{
	unsigned i, j;

	for (i = 0; i < s_levels.used; i++)
	{
		struct level_t *l = s_levels.items[i];

		if (l == NULL || l->changed) continue;

		/* Test if another thread is accessing... */
		if (pthread_mutex_trylock(&l->mutex) != 0) continue;
		pthread_mutex_unlock(&l->mutex);

		if (!level_is_empty(l)) continue;

		bool cuboid = false;
		for (j = 0; j < s_cuboids.used; j++)
		{
			const struct cuboid_t *c = &s_cuboids.items[j];
			if (c->level == l)
			{
				cuboid = true;
				break;
			}
		}

		if (cuboid) continue;

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

void level_cuboid(struct level_t *level, unsigned start, unsigned end, enum blocktype_t old_type, enum blocktype_t new_type, const struct player_t *p)
{
	struct cuboid_t c;

	if (!level_get_xyz(level, start, &c.sx, &c.sy, &c.sz)) return;
	if (!level_get_xyz(level, end, &c.ex, &c.ey, &c.ez)) return;

	int16_t t;
	if (c.ex < c.sx) { t = c.sx; c.sx = c.ex; c.ex = t; }
	if (c.ey < c.sy) { t = c.sy; c.sy = c.ey; c.ey = t; }
	if (c.ez < c.sz) { t = c.sz; c.sz = c.ez; c.ez = t; }

	c.cx = c.sx;
	c.cy = c.ey;
	c.cz = c.sz;
	c.level = level;
	c.count = 0;
	c.old_type = old_type;
	c.new_type = new_type;
	c.owner = HasBit(p->flags, FLAG_DISOWN) ? 0 : p->globalid;
	c.owner_is_op = p->rank >= RANK_OP;
	c.fixed = HasBit(p->flags, FLAG_PLACE_FIXED);

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

	if (click)
	{
		/* Check block distcance */
		int distance = abs(client->player->pos.x / 32 - x);
		distance += abs(client->player->pos.y / 32 - y);
		distance += abs(client->player->pos.z / 32 - z);

		if (distance > 12)
		{
			char buf[64];
			snprintf(buf, sizeof buf, TAG_RED "Kicked %s for building too far away", client->player->username);
			net_notify_all(buf);
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

	if (click && client->player->mode == MODE_INFO)
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

	if (click && client->player->mode == MODE_CUBOID && can_build)
	{
		client_add_packet(client, packet_send_set_block(x, y, z, convert(level, index, b)));

		if (client->player->cuboid_start == UINT_MAX)
		{
			client->player->cuboid_start = index;
			client_notify(client, "Cuboid start placed");
			return;
		}

		client_notify(client, "Cuboid end placed");
		level_cuboid(level, client->player->cuboid_start, index, -1, client->player->cuboid_type == BLOCK_INVALID ? client->player->bindings[t] : client->player->cuboid_type, client->player);
		client->player->mode = MODE_NORMAL;
		return;
	}
	else if (click && client->player->mode == MODE_REPLACE && can_build)
	{
		client_add_packet(client, packet_send_set_block(x, y, z, convert(level, index, b)));

		if (client->player->cuboid_start == UINT_MAX)
		{
			client->player->cuboid_start = index;
			client_notify(client, "Replace start placed");
			return;
		}

		client_notify(client, "Replace end placed");
		level_cuboid(level, client->player->cuboid_start, index, client->player->replace_type, client->player->cuboid_type == BLOCK_INVALID ? client->player->bindings[t] : client->player->cuboid_type, client->player);
		client->player->mode = MODE_NORMAL;
		return;
	}

	enum blocktype_t nt = click ? client->player->bindings[t] : t;

	if (click)
	{
		if (client->player->mode == MODE_PLACE_SOLID) nt = ADMINIUM;
		else if (client->player->mode == MODE_PLACE_WATER) nt = WATERSTILL;
		else if (client->player->mode == MODE_PLACE_LAVA) nt = LAVASTILL;
	}

	/* Client thinks it has changed to air */
	if (m == 0) t = AIR;
	if (click && !HasBit(client->player->flags, FLAG_PAINT))
	{
		if (m == 0) {
			int r = trigger(level, index, b);
			if (r > 0)
			{
				if (r == 2) client_add_packet(client, packet_send_set_block(x, y, z, convert(level, index, b)));

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
		client_notify(client, "You do not have sufficient permission to build on this level");
		client_add_packet(client, packet_send_set_block(x, y, z, convert(level, index, b)));
		return;
	}

	if (client->player->globalid != level->owner)
	{
		/* Not level owner, so check block permissions */

		if (client->player->rank < RANK_OP && (bt == ADMINIUM || nt == ADMINIUM || b->fixed))
			// || (b->owner != 0 && b->owner != client->player->globalid)))
		{
			client_notify(client, "Block cannot be changed");
			client_add_packet(client, packet_send_set_block(x, y, z, convert(level, index, b)));
			return;
		}

		/* Air with owner can be replaced */
		if (client->player->rank < RANK_OP && (b->owner != 0 && b->owner != client->player->globalid && b->type != AIR))
		{
			client_add_packet(client, packet_send_set_block(x, y, z, convert(level, index, b)));
			return;
		}
	}

	if (bt != nt)
	{
		if (level->undo == NULL)
		{
			level->undo = undodb_init(level->name);
		}
		undodb_log(level->undo, client->player->globalid, x, y, z, b->type, b->data, nt);
//		player_undo_log(client->player, index);

		delete(level, index, b);

		bool oldphysics = b->physics;

		b->type = nt;
		b->data = 0;
		b->fixed = HasBit(client->player->flags, FLAG_PLACE_FIXED);
		b->owner = HasBit(client->player->flags, FLAG_DISOWN) ? 0 : client->player->globalid;
		b->physics = blocktype_has_physics(nt);

		if (oldphysics != b->physics)
		{
			if (oldphysics) physics_list_del_item(&level->physics, index);
			if (b->physics) {
				physics_list_add(&level->physics, index);
				//level_addupdate(level, index, BLOCK_INVALID, 0);
			}
		}

		level->changed = true;

		enum blocktype_t pt = convert(level, index, b);

		unsigned i;
		for (i = 0; i < s_clients.used; i++)
		{
			struct client_t *c = s_clients.items[i];
			if (c->player == NULL) continue;
			if ((client != c || pt != t || !click) && c->player->level == level)
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

static void level_mark_teleporter(struct level_t *level, struct position_t *pos)
{
	unsigned index = level_get_index(level, pos->x / 32, pos->y / 32, pos->z / 32);
	level->blocks[index].teleporter = 1;
}

static void level_unmark_teleporter(struct level_t *level, struct position_t *pos)
{
	unsigned index = level_get_index(level, pos->x / 32, pos->y / 32, pos->z / 32);
	level->blocks[index].teleporter = 0;
}

static int gettime()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
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
		LOG("Physics ran in %dms (%lu blocks)\n", level->physics_runtime, level->physics2.used);
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

	   // LOG("Starting update run with %lu blocks\n", level->updates.used)

		level->updates_runtime = 0;

		int i;
		for (i = 0; i < level->updates.used; i++)
		{
			struct block_update_t *bu = &level->updates.items[i];
			struct block_t *b = &level->blocks[bu->index];
			b->touched = b->type != bu->oldtype;
		}
	}

	//LOG("Done %d out of %lu\n", level->updates_iter, level->updates.used);

	int n = 40;
	for (; level->updates_iter < level->updates.used; level->updates_iter++)
	{
		struct block_update_t *bu = &level->updates.items[level->updates_iter];
		struct block_t *b = &level->blocks[bu->index];

		/* Already touched this block */
		if (b->touched) continue;

		enum blocktype_t pt1 = convert(level, bu->index, b);

		if (bu->newtype != BLOCK_INVALID)
		{
			b->type = bu->newtype;
		}

		b->touched = true;
		b->data = bu->newdata;
		b->physics = blocktype_has_physics(b->type);
		if (b->physics) physics_list_add(&level->physics, bu->index);

		enum blocktype_t pt2 = convert(level, bu->index, b);
		int16_t x, y, z;
		level_get_xyz(level, bu->index, &x, &y, &z);

		/* Don't send client updates when in "no limits" or instant mode */
		if (limit && !level->instant && pt1 != pt2)
		{
			unsigned j;
			for (j = 0; j < s_clients.used; j++)
			{
				struct client_t *c = s_clients.items[j];
				if (c->player == NULL) continue;
				if (c->player->level == level)
				{
					client_add_packet(c, packet_send_set_block(x, y, z, pt2));
				}
			}

			/* Max changes */
			n--;
			if (n == 0) {
				level->updates_runtime += gettime() - s;
				return;
			}
		}

		/* Max iterations, or 40 ms */
		//if (gettime() - s > 40) return;
	}

	level->updates_runtime += gettime() - s;

	if (level->updates_runtime > 50)
	{
		LOG("Updates ran in %dms (%lu blocks)\n", level->updates_runtime, level->updates.used);
	}

	level->updates_runtime_last = level->updates_runtime;
	level->updates_count_last = level->updates.used;

	//LOG("Hmm (%lu / %lu blocks)\n", level->physics.used, level->physics2.used);

	level->updates.used = 0;
	level->updates_iter = 0;
	level->physics_iter = 0;

	level->physics_done = 0;

	if (limit)
	{
		call_level_hook(EVENT_TICK, level, NULL, NULL);
	}
}

void level_addupdate(struct level_t *level, unsigned index, enum blocktype_t newtype, uint16_t newdata)
{
	struct block_update_t bu;
	bu.index = index;
	bu.oldtype = level->blocks[index].type;
	bu.newtype = newtype;
	bu.newdata = newdata;
	block_update_list_add(&level->updates, bu);
}

void level_addupdate_override(struct level_t *level, unsigned index, enum blocktype_t newtype, uint16_t newdata)
{
	/* Time sink? */
	unsigned i;
	for (i = 0; i < level->updates.used; i++)
	{
		struct block_update_t *bu = &level->updates.items[i];
		if (index == bu->index) {
			/* Override update */
			if (bu->newtype == WATER)
			{
				bu->newtype = newtype;
				bu->newdata = newdata;
			}
			return;
		}
	}

	struct block_update_t bu;
	bu.index = index;
	bu.oldtype = level->blocks[index].type;
	bu.newtype = newtype;
	bu.newdata = newdata;
	block_update_list_add(&level->updates, bu);
}

void level_prerun(struct level_t *l)
{
	int n;

	for (n = 0; n < 100; n++)
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

		/* Don't run physics for empty levels, else it will never unload */
		if (level_is_empty(level)) continue;

		/* Test if another thread is accessing... */
		if (pthread_mutex_trylock(&level->mutex) != 0) continue;
		pthread_mutex_unlock(&level->mutex);

		level_run_physics(level, can_init, true);
	}
}

void level_process_updates(bool can_init)
{
	unsigned i;
	for (i = 0; i < s_levels.used; i++)
	{
		struct level_t *level = s_levels.items[i];
		if (level == NULL) continue;

		/* Don't run physics for empty levels, else it will never unload */
		if (level_is_empty(level)) continue;

		/* Test if another thread is accessing... */
		if (pthread_mutex_trylock(&level->mutex) != 0) continue;
		pthread_mutex_unlock(&level->mutex);

		level_run_updates(level, can_init, true);
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

void register_level_hook_func(const char *name, level_hook_func_t level_hook_func)
{
	struct level_hook_t lh;
	strncpy(lh.name, name, sizeof lh.name);
	lh.level_hook_func = level_hook_func;

	level_hook_list_add(&s_level_hooks, lh);

	LOG("Registered level hook %s\n", name);

	unsigned i;
	for (i = 0; i < s_levels.used; i++)
	{
		struct level_t *l = s_levels.items[i];
		if (l == NULL) continue;
		if (l->level_hook_func == NULL && strcasecmp(l->level_hook_name, name) == 0)
		{
			level_hook_attach(l, name);
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
		if (l->level_hook_func != NULL && strcasecmp(l->level_hook_name, name) == 0)
		{
			l->level_hook_func = NULL;
			LOG("Detached level hook %s from %s\n", name, l->name);
		}
	}

	level_hook_list_del_item(&s_level_hooks, lh);
	LOG("Deregistered level hook %s\n", name);
}

void level_hooks_deinit()
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
	unsigned i;
	for (i = 0; i < s_level_hooks.used; i++)
	{
		if (strcasecmp(s_level_hooks.items[i].name, name) == 0)
		{
			strcpy(l->level_hook_name, s_level_hooks.items[i].name);
			l->level_hook_func = s_level_hooks.items[i].level_hook_func;
			LOG("Attached level hook %s to %s\n", name, l->name);
			call_level_hook(EVENT_INIT, l, NULL, NULL);
			return true;
		}
	}

	return false;
}

bool level_hook_detach(struct level_t *l, const char *name)
{
	unsigned i;
	for (i = 0; i < 1; i++)
	{
		if (strcasecmp(l->level_hook_name, name) == 0)
		{
			LOG("Detached level hook %s from %s\n", name, l->name);
			l->level_hook_func = NULL;
			return true;
		}
	}

	return false;
}

void call_level_hook(int hook, struct level_t *l, struct client_t *c, void *data)
{
	if (l->level_hook_func == NULL) return;

	l->level_hook_func(hook, l, c, data, &l->level_hook_data);
}
