#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <zlib.h>
#include <pthread.h>
#include <math.h>
#include "level.h"
#include "block.h"
#include "client.h"
#include "cuboid.h"
#include "mcc.h"
#include "packet.h"
#include "player.h"
#include "playerdb.h"
#include "network.h"
#include "util.h"

struct level_list_t s_levels;

bool level_t_compare(struct level_t **a, struct level_t **b)
{
    return *a == *b;
}

bool level_init(struct level_t *level, unsigned x, unsigned y, unsigned z, const char *name)
{
    memset(level, 0, sizeof *level);

    if (name != NULL)
    {
        strncpy(level->name, name, sizeof level->name);
    }

	level->x = x;
	level->y = y;
	level->z = z;

	level->blocks = calloc(x * y * z, sizeof *level->blocks);
	if (level->blocks == NULL) return false;

	physics_list_init(&level->physics);

	return true;
}

void level_set_block(struct level_t *level, struct block_t *block, unsigned index)
{
	bool old_phys = block_has_physics(&level->blocks[index]);
	bool new_phys = block_has_physics(block);

	level->blocks[index] = *block;

	if (new_phys != old_phys)
	{
		if (new_phys)
		{
			physics_list_add(&level->physics, index);
		}
		else
		{
			physics_list_del_item(&level->physics, index);
		}
	}
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



bool level_send(struct client_t *c)
{
    struct level_t *oldlevel = c->player->level;
    struct level_t *newlevel = c->player->new_level;
    unsigned length = newlevel->x * newlevel->y * newlevel->z;
    int x;
    int i;
    z_stream z;

    /* If we can't lock the mutex then the thread is already locked */
    if (pthread_mutex_trylock(&newlevel->mutex))
    {
        if (!c->waiting_for_level)
        {
            client_notify(c, "Please wait for level generation to complete");
            c->waiting_for_level = true;
        }
        return false;
    }
    pthread_mutex_unlock(&newlevel->mutex);

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
            client_notify(c, "Uh, level is full, sorry...");
            return false;
        }
    }

    memset(&z, 0, sizeof z);
    if (deflateInit2(&z, 5, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK) return false;

    uint8_t *buffer = malloc(4 + length);
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
            *bufp++ = (newlevel->blocks[x].owner == c->player->filter) ? block_get_blocktype(&newlevel->blocks[x]) : AIR;
        }
        else
        {
            *bufp++ = block_get_blocktype(&newlevel->blocks[x]);
        }
    }

    if (oldlevel != NULL)
    {
        if (oldlevel != newlevel)
        {
            char buf[64];
            snprintf(buf, sizeof buf, TAG_WHITE "=%s" TAG_YELLOW " moved to '%s'", c->player->colourusername, newlevel->name);
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

static const float _amplitudes[][7] = {
    {16000,   256,   128,    64,    32,    16,    16},
    /* Very smooth */
    {16000,  5600,  1968,   688,   240,    16,    16},
    /* Smooth */
    {16000, 16000,  6448,  3200,  1024,   128,    16},
    /* Rough */
    {16000, 19200, 12800,  8000,  3200,   256,    64},
    /* Very Rough */
    {24000, 16000, 19200, 16000,  8000,   512,   320},
    /* Huh */
    {1, 1, 1, 1, 1, 1, 1},
};

static float random_height(int max)
{
    return ((float)rand() / INT_MAX - 0.5f) * max;
}

static bool level_apply_noise(float *map, int mx, int mz, int log_frequency, int amplitude)
{
    int size_min = mx < mz ? mx : mz;
    int step = size_min >> log_frequency;
    int x, z;
    int dmx = mx + 1;

    if (step == 0) return false;

    if (log_frequency == 0)
    {
        for (z = 0; z <= mz; z += step)
        {
            for (x = 0; x <= mx; x += step)
            {
                float h = (amplitude > 0) ? random_height(amplitude) : 0;
                map[x + z * dmx] = h;
            }
        }
        return true;
    }

    for (z = 0; z <= mz; z += 2 * step)
    {
        for (x = 0; x < mx; x += 2 * step)
        {
            float h0 = map[x + z * dmx];
            float h2 = map[x + step + step + z * dmx];
            float h1 = (h0 + h2) / 2.0f;
            map[x + step + z * dmx] = h1;
        }
    }

    for (z = 0; z < mz; z += 2 * step)
    {
        for (x = 0; x <= mx; x += step)
        {
            float h0 = map[x + z * dmx];
            float h2 = map[x + (z + step + step) * dmx];
            float h1 = (h0 + h2) / 2.0f;
            map[x + (z + step) * dmx] = h1;
        }
    }

    for (z = 0; z <= mz; z += step)
    {
        for (x = 0; x <= mx; x += step)
        {
            map[x + z * dmx] += random_height(amplitude);
        }
    }

    return (step > 1);
}

static void level_normalize(float *map, int mx, int mz)
{
    int x, z;
    float hmin = 0.0f;
    float hmax = 0.0f;
    int dmx = mx + 1;

    for (z = 0; z <= mz; z++)
    {
        for (x = 0; x <= mx; x++)
        {
            float h = map[x + z * dmx];
            if (h < hmin) hmin = h;
            if (h > hmax) hmax = h;
        }
    }

    for (z = 0; z <= mz; z++)
    {
        for (x = 0; x <= mx; x++)
        {
            float h = map[x + z * dmx];
            h -= hmin;
            h /= (hmax - hmin);
            h = h < 0.0 ? 0.0 : h >= 1.0f ? 1.0f : h;//(asin(h * 2.0f - 1.0f) / M_PI + 0.5f);
            if (h < 0.5) {
                h = sin((h - 0.25f) * 2 * M_PI) / 4.0f + 0.25f;
            }
            else
            {
                h = sin((h - 0.75f) * 2 * M_PI) / 4.0f + 0.75f;
            }
            map[x + z * dmx] = h;
        }
    }
}

/*
static void level_smooth_slopes(float *map, int mx, int mz, float dh_max)
{
    int x, z;
    int dmx = mx + 1;
    for (z = 0; z <= mz; z++)
    {
        for (x = 0; x <= mx; x++)
        {
            float h_max = map[(x > 0 ? x - 1 : 0) + z * dmx];
            float h_max2 = map[x + (z > 0 ? z - 1 : 0) * dmx];
            if (h_max2 < h_max) h_max = h_max2;
            h_max += 1.0f / dh_max;
            if (map[x + z * dmx] > h_max) map[x + z * dmx] = h_max;
        }
    }
    for (z = mz; z >= 0; z--)
    {
        for (x = mx; x >= 0; x--)
        {
            float h_max = map[(x < mx ? x + 1 : x) + z * dmx];
            float h_max2 = map[x + (z < mz ? z + 1 : z) * dmx];
            if (h_max2 < h_max) h_max = h_max2;
            h_max += 1.0f / dh_max;
            if (map[x + z * dmx] > h_max) map[x + z * dmx] = h_max;
        }
    }
}
*/

static void level_gen_heightmap(float *map, int mx, int mz, int type)
{
    unsigned iteration_round = 0;
    bool continue_iteration;
    int log_size_min;
    int log_frequency_min;
    int log_frequency;
    int amplitude;
    int size_min = mx < mz ? mx : mz;

    for (log_size_min = 6; (1U << log_size_min) < size_min; log_size_min++) { }
    log_frequency_min = log_size_min - 6;

    do {
        log_frequency = iteration_round - log_frequency_min;
        if (log_frequency >= 0) {
            amplitude = _amplitudes[type][log_frequency];
        } else {
            amplitude = 0;
        }

        continue_iteration = level_apply_noise(map, mx, mz, iteration_round, amplitude);
        iteration_round++;
    } while (continue_iteration);

    level_normalize(map, mx, mz);
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

    memset(&block, 0, sizeof block);

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
        float *hm = malloc((mx + 1) * (mz + 1) * sizeof *hm);
        float *cmh = malloc((mx + 1) * (mz + 1) * sizeof *cmh);
        float *cmd = malloc((mx + 1) * (mz + 1) * sizeof *cmd);

        level_gen_heightmap(hm, mx, mz, level->type - 2);
        //level_smooth_slopes(hm, mx, mz, my);

        int dmx = mx + 1;

        for (z = 0; z < mz; z++)
        {
            for (x = 0; x < mx; x++)
            {
                int h = hm[x + z * dmx] * my / 4 + my * 3 / 8;

                for (y = 0; y < h; y++)
                {
                    block.type = (y < h - 5) ? ROCK : (y < h - 1) ? DIRT : (y < my / 2) ? SAND : GRASS;
                    level_set_block(level, &block, level_get_index(level, x, y, z));
                }

                block.type = WATER;
                for (; y < my / 2; y++)
                {
                    level_set_block(level, &block, level_get_index(level, x, y, z));
                }
            }
        }

        block.type = AIR;
        for (i = 0; i < my / 4; i++)
        {
            level_gen_heightmap(cmh, mx, mz, level->type - 2);
            level_gen_heightmap(cmd, mx, mz, level->type - 2);

            int base = cmd[0] * my;

            for (z = 0; z < mz; z++)
            {
                for (x = 0; x < mx; x++)
                {
                    //int h = hm[x + z * dmx] * my / 2 + my / 3;
                    float ch = cmh[x + z * dmx];
                    float cd = cmd[x + z * dmx];
                    if (fabsf(ch) < 0.25f)
                    {
                        //block.type = i % 3 ? AIR : ROCK;
                        //int cdi = h - cd * my / 4;
                        //int chi = cdi + (ch - 0.5f) * my / 4;
                        int cdi = base + cd * my / 8;// / 1.5f;
                        int chi = ch * my / 4 + cdi;//di + (ch - 0.5f) * my / 3.0f;
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

        free(hm);
        free(cmd);
        free(cmh);

        block.type = WATER;
        for (i = 0; i < 100; i++)
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

    level->spawn.x = mx * 16;
    level->spawn.z = mz * 16;
    for (y = my - 5; y > 0; y--)
    {
        unsigned index = level_get_index(level, x, y, z);
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


    level->changed = true;

    pthread_mutex_unlock(&level->mutex);

    char buf[64];
    snprintf(buf, sizeof buf, "Created level '%s'", level->name);
    net_notify_all(buf);

    //LOG(buf);

    return NULL;
}

void level_gen(struct level_t *level, int type)
{
    level->type = type;

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
    char buf[64];
    snprintf(buf, sizeof buf, "Level '%s' unloaded", level->name);
    net_notify_all(buf);

    //LOG(buf);

    physics_list_free(&level->physics);

    free(level->blocks);
    free(level);
}

static void *level_load_thread_abort(struct level_t *level)
{
    LOG("Unable to load level %s", level->name);

    int i;
    for (i = 0; i < s_clients.used; i++)
    {
        struct client_t *c = s_clients.items[i];
        printf("client %d: player %p\n", i, c->player);
        if (c->player == NULL) continue;
        printf("level %p, new_level %p\n", level, c->player->new_level);
        if (c->player->new_level == level)
        {
            c->player->new_level = c->player->level;
            c->waiting_for_level = false;
            LOG("Aborted level change for %s", c->player->username);
        }
    }

    pthread_mutex_unlock(&level->mutex);

    return NULL;
}

static void *level_load_thread(void *arg)
{
    struct level_t *l = arg;
    gzFile gz;

    char name[64];
    strncpy(name, l->name, sizeof name);

    char filename[64];
    snprintf(filename, sizeof filename, "levels/%s.%s", name, l->convert ? "lvl" : "mcl");
    lcase(filename);

    gz = gzopen(filename, "rb");
    if (gz == NULL) return level_load_thread_abort(l);

    if (l->convert)
    {
   	    int16_t x, y, z;
    	int16_t version;

		if (gzread(gz, &version, sizeof version) != sizeof version) return level_load_thread_abort(l);
		if (version == 1874)
		{
			if (gzread(gz, &x, sizeof x) != sizeof x) return level_load_thread_abort(l);
		}
		else
		{
			x = version;
		}
		if (gzread(gz, &z, sizeof z) != sizeof y) return level_load_thread_abort(l);
		if (gzread(gz, &y, sizeof y) != sizeof z) return level_load_thread_abort(l);
		if (!level_init(l, x, y, z, name)) return level_load_thread_abort(l);
		if (gzread(gz, &l->spawn.x, sizeof l->spawn.x) != sizeof l->spawn.x) return level_load_thread_abort(l);
		if (gzread(gz, &l->spawn.z, sizeof l->spawn.z) != sizeof l->spawn.z) return level_load_thread_abort(l);
		if (gzread(gz, &l->spawn.y, sizeof l->spawn.y) != sizeof l->spawn.y) return level_load_thread_abort(l);
		if (gzread(gz, &l->spawn.h, sizeof l->spawn.h) != sizeof l->spawn.h) return level_load_thread_abort(l);
		if (gzread(gz, &l->spawn.p, sizeof l->spawn.p) != sizeof l->spawn.p) return level_load_thread_abort(l);

		l->spawn.x *= 32;
		l->spawn.y *= 32;
		l->spawn.z *= 32;

		if (version == 1874)
		{
			uint8_t pervisit, perbuild;
			if (gzread(gz, &pervisit, sizeof pervisit) != sizeof pervisit) return level_load_thread_abort(l);
			if (gzread(gz, &perbuild, sizeof perbuild) != sizeof perbuild) return level_load_thread_abort(l);
		}

		size_t s = x * y * z;
		uint8_t *blocks = malloc(s);
		if (blocks == NULL) return level_load_thread_abort(l);
		if (gzread(gz, blocks, s) != s)
		{
			free(blocks);
			return level_load_thread_abort(l);
		}

		int i;
		for (i = 0; i < s; i++)
		{
			l->blocks[i] = block_convert_from_mcs(blocks[i]);
		}

		free(blocks);
    }
    else
	{
		unsigned x, y, z;
		if (gzread(gz, &x, sizeof x) != sizeof x) return level_load_thread_abort(l);
		if (gzread(gz, &y, sizeof y) != sizeof y) return level_load_thread_abort(l);
		if (gzread(gz, &z, sizeof z) != sizeof z) return level_load_thread_abort(l);
		if (!level_init(l, x, y, z, name)) return level_load_thread_abort(l);

		if (gzread(gz, &l->spawn, sizeof l->spawn) != sizeof l->spawn) return level_load_thread_abort(l);

		size_t s = sizeof *l->blocks * x * y *z;
		if (gzread(gz, l->blocks, s) != s) return level_load_thread_abort(l);
	}

    gzclose(gz);

    char buf[64];
    snprintf(buf, sizeof buf, "Level '%s' loaded", l->name);

    //LOG("%s\n", buf);

    pthread_mutex_unlock(&l->mutex);
    //if (level != NULL) *level = l;

    net_notify_all(buf);

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

    char filename[64];
    snprintf(filename, sizeof filename, "levels/%s.mcl", l->name);
    lcase(filename);

    gzFile gz = gzopen(filename, "wb");
    if (gz == NULL)
    {
        pthread_mutex_unlock(&l->mutex);
        return NULL;
    }

    gzwrite(gz, &l->x, sizeof l->x);
    gzwrite(gz, &l->y, sizeof l->y);
    gzwrite(gz, &l->z, sizeof l->z);
    gzwrite(gz, &l->spawn, sizeof l->spawn);
    gzwrite(gz, l->blocks, sizeof *l->blocks * l->x * l->y * l->z);
    gzclose(gz);

    //LOG("Level '%s' saved\n", l->name);

    char buf[64];
    snprintf(buf, sizeof buf, "Level '%s' saved", l->name);
    net_notify_all(buf);

    pthread_mutex_unlock(&l->mutex);

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

bool level_get_by_name(const char *name, struct level_t **level)
{
    int i;
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

void level_save_all()
{
    int i;

    for (i = 0; i < s_levels.used; i++)
    {
        if (s_levels.items[i] != NULL && s_levels.items[i]->changed)
        {
            level_save(s_levels.items[i]);
        }
    }
}

void level_unload_empty()
{
    int i, j;

    for (i = 0; i < s_levels.used; i++)
    {
        struct level_t *l = s_levels.items[i];

        if (l == NULL || l->changed) continue;

        /* Test if another thread is accessing... */
        if (pthread_mutex_trylock(&l->mutex) != 0) continue;
        pthread_mutex_unlock(&l->mutex);

        bool player_on = false;
        for (j = 0; j < s_clients.used; j++)
        {
            const struct player_t *p = s_clients.items[j]->player;
            if (p == NULL) continue;
            if (p->level == l || p->new_level == l)
            {
                player_on = true;
                break;
            }
        }

        for (j = 0; j < s_cuboids.used; j++)
        {
            const struct cuboid_t *c = &s_cuboids.items[j];
            if (c->level == l)
            {
                player_on = true;
                break;
            }
        }

        if (player_on) continue;

        level_unload(l);
        //level_list_del(&s_levels, l);

        s_levels.items[i] = NULL;
    }
}

bool level_get_xyz(const struct level_t *level, unsigned index, int16_t *x, int16_t *y, int16_t *z)
{
	if (index < 0 || index >= level->x * level->y * level->z) return false;

	if (x != NULL) *x = index % level->x;
    if (y != NULL) *y = index / level->x / level->z;
    if (z != NULL) *z = (index / level->x) % level->z;

    return true;
}

static void level_cuboid(struct level_t *level, unsigned start, unsigned end, enum blocktype_t type)
{
    struct cuboid_t c;

	if (!level_get_xyz(level, start, &c.sx, &c.sy, &c.sz)) return;
	if (!level_get_xyz(level, end, &c.ex, &c.ey, &c.ez)) return;

	int16_t t;
	if (c.ex < c.sx) { t = c.sx; c.sx = c.ex; c.ex = t; }
	if (c.ey < c.sy) { t = c.sy; c.sy = c.ey; c.ey = t; }
	if (c.ez < c.sz) { t = c.sz; c.sz = c.ez; c.ez = t; }

	c.cx = c.sx;
	c.cy = c.sy;
	c.cz = c.sz;
	c.level = level;
	c.count = 0;
	c.new_type = type;

	cuboid_list_add(&s_cuboids, c);
}

void level_change_block(struct level_t *level, struct client_t *client, int16_t x, int16_t y, int16_t z, uint8_t m, uint8_t t)
{
    int i;

    if (client->player->rank == RANK_BANNED)
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
    enum blocktype_t bt = block_get_blocktype(b);

    if (client->player->mode == MODE_INFO)
    {
        char buf[64];
        snprintf(buf, sizeof buf, "%s%s at %dx%dx%d placed by %s",
                 b->fixed ? "fixed " : "",
                 blocktype_get_name(bt), x, y, z,
                 b->owner == 0 ? "none" : playerdb_get_username(b->owner)
        );
        client_notify(client, buf);
        client_add_packet(client, packet_send_set_block(x, y, z, bt));
        return;
    }
    else if (client->player->mode == MODE_CUBOID)
    {
    	client_add_packet(client, packet_send_set_block(x, y, z, bt));

		if (client->player->cuboid_start == UINT_MAX)
		{
			client->player->cuboid_start = index;
			client_notify(client, "Cuboid start placed");
			return;
		}

		client_notify(client, "Cuboid end placed");
		level_cuboid(level, client->player->cuboid_start, index, client->player->cuboid_type == -1 ? t : client->player->cuboid_type);
		client->player->mode = MODE_NORMAL;
		return;
    }

    enum blocktype_t nt = t;

    if (client->player->mode == MODE_PLACE_SOLID) nt = ADMINIUM;
    else if (client->player->mode == MODE_PLACE_WATER) nt = WATER;
    else if (client->player->mode == MODE_PLACE_LAVA) nt = LAVA;

    if (m == 0) nt = AIR;

    if (client->player->rank < RANK_OP && (bt == ADMINIUM || nt == ADMINIUM || b->fixed))
        // || (b->owner != 0 && b->owner != client->player->globalid)))
    {
        client_notify(client, "Block cannot be changed");
        client_add_packet(client, packet_send_set_block(x, y, z, bt));
        return;
    }

    if (client->player->rank < RANK_OP && (b->owner != 0 && b->owner != client->player->globalid))
    {
        client_add_packet(client, packet_send_set_block(x, y, z, bt));
        return;
    }

    if (bt != nt)
    {
        player_undo_log(client->player, index);

        b->type = nt;
        b->fixed = HasBit(client->player->flags, FLAG_PLACE_FIXED);
        b->owner = (b->type == AIR && !b->fixed) ? 0 : client->player->globalid;

        level->changed = true;

        for (i = 0; i < s_clients.used; i++)
        {
            struct client_t *c = s_clients.items[i];
            if (c->player == NULL) continue;
            if ((client != c || (nt != t && m == 1)) && c->player->level == level)
            {
                client_add_packet(c, packet_send_set_block(x, y, z, nt));
            }
        }
    }
}

void level_change_block_force(struct level_t *level, struct block_t *block, unsigned index)
{
    int i;
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
            client_add_packet(c, packet_send_set_block(x, y, z, block->type));
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

void level_set_teleporter(struct level_t *level, const char *name, struct position_t *pos, const char *dest, const char *dest_level)
{
    int i = 0;
    for (i = 0; i < level->teleporters.used; i++)
    {
        struct teleporter_t *t = &level->teleporters.items[i];
        if (strcasecmp(t->name, name) == 0)
        {
            level_unmark_teleporter(level, &t->pos);
            t->pos = *pos;
            if (dest != NULL) strncpy(t->dest, dest, sizeof t->dest);
            if (dest_level != NULL) strncpy(t->dest_level, dest_level, sizeof t->dest_level);
            level_mark_teleporter(level, pos);
            return;
        }
    }

    struct teleporter_t t;
    memset(&t, 0, sizeof t);
    strncpy(t.name, name, sizeof t.name);
    t.pos = *pos;
    if (dest != NULL) strncpy(t.dest, dest, sizeof t.dest);
    if (dest_level != NULL) strncpy(t.dest_level, dest_level, sizeof t.dest_level);

    teleporter_list_add(&level->teleporters, t);
    level_mark_teleporter(level, pos);
}
