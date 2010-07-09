#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <zlib.h>
#include <pthread.h>
#include "level.h"
#include "block.h"
#include "client.h"
#include "packet.h"
#include "player.h"

#define M_PI 3.14159265358979f

struct level_list_t s_levels;

bool level_t_compare(struct level_t **a, struct level_t **b)
{
    return *a == *b;
}

void level_init(struct level_t *level, unsigned x, unsigned y, unsigned z, const char *name)
{
    memset(level, 0, sizeof *level);

    level->name = strdup(name);
	level->x = x;
	level->y = y;
	level->z = z;

	level->blocks = calloc(x * y * z, sizeof *level->blocks);

	physics_list_init(&level->physics);
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
			physics_list_del(&level->physics, index);
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

void level_clear_block(struct level_t *level, unsigned index)
{
	static struct block_t empty = { AIR, 0 };

	level_set_block(level, &empty, index);
}

bool level_send(struct client_t *c)
{
    struct level_t *level = c->player->level;
    unsigned length = level->x * level->y * level->z;
    int x;
    z_stream z;

    c->waiting_for_level = true;

    /* If we can't lock the mutex then the thread is already locked */
    if (pthread_mutex_trylock(&level->mutex))
    {
        return false;
    }
    pthread_mutex_unlock(&level->mutex);

    memset(&z, 0, sizeof z);
    if (deflateInit2(&z, 5, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK) return false;

    uint8_t *buffer = malloc(4 + length);
    uint8_t *bufp = buffer;
    *bufp++ = (length >> 24) & 0xFF;
    *bufp++ = (length >> 16) & 0xFF;
    *bufp++ = (length >>  8) & 0xFF;
    *bufp++ =  length        & 0xFF;

    for (x = 0; x < length; x++)
    {
        *bufp++ = block_get_blocktype(&level->blocks[x]);
    }

    uint8_t outbuf[1024];

    client_add_packet(c, packet_send_level_initialize());

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

    client_add_packet(c, packet_send_level_finalize(level->x, level->y, level->z));

    client_add_packet(c, packet_send_teleport_player(0xFF, level->spawnx, level->spawny, level->spawnz, level->spawnh, level->spawnp));

    c->waiting_for_level = false;

    return true;
}

static const float _amplitudes[] = {
    16000, 19200, 12800, 8000, 3200, 256, 64
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
            map[x + z * dmx] = h < 0.0 ? 0.0 : h >= 1.0f ? 1.0f : (asin(h * 2.0f - 1.0f) / M_PI + 0.5f);
        }
    }
}

static void level_gen_heightmap(float *map, int mx, int mz)
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
            amplitude = _amplitudes[log_frequency];
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

    float *hm = malloc((mx + 1) * (mz + 1) * sizeof *hm);
    float *cmh = malloc((mx + 1) * (mz + 1) * sizeof *cmh);
    float *cmd = malloc((mx + 1) * (mz + 1) * sizeof *cmd);

    level_gen_heightmap(hm, mx, mz);
    level_gen_heightmap(cmh, mx, mz);
    level_gen_heightmap(cmd, mx, mz);

    int dmx = mx + 1;

    for (z = 0; z < mz; z++)
    {
        for (x = 0; x < mx; x++)
        {
            int h = hm[x + z * dmx] * my / 2 + my / 3;

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
    for (i = 0; i < 50; i++)
    {
        level_gen_heightmap(cmh, mx, mz);
        level_gen_heightmap(cmd, mx, mz);

        int base = cmd[0] * my;

        for (z = 0; z < mz; z++)
        {
            for (x = 0; x < mx; x++)
            {
                //int h = hm[x + z * dmx] * my / 2 + my / 3;
                float ch = cmh[x + z * dmx];
                float cd = cmd[x + z * dmx];
                if (fabs(ch) < 0.25f)
                {
                    //block.type = i % 3 ? AIR : ROCK;
                    //int cdi = h - cd * my / 4;
                    //int chi = cdi + (ch - 0.5f) * my / 4;
                    int cdi = base + cd * my / 8;// / 1.5f;
                    int chi = (ch - 0.25f) * my / 4 + cdi;//di + (ch - 0.5f) * my / 3.0f;
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

    block.type = WATER;
    for (i = 0; i < 100; i++)
    {
        level_gen_heightmap(cmh, mx, mz);
        level_gen_heightmap(cmd, mx, mz);

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

    level->spawnx = mx * 16;
    level->spawnz = mz * 16;
    for (y = my - 1; y > 0; y--)
    {
        unsigned index = level_get_index(level, x, y, z);
        if (level->blocks[index].type != AIR)
        {
            level->spawny = (y + 2) * 32;
            break;
        }
    }

    level->spawnh = 90 * 256 / 360;
    level->spawnp = 90 * 256 / 360;

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

    free(hm);
    free(cmd);
    free(cmh);

    level->changed = true;

    pthread_mutex_unlock(&level->mutex);

    return NULL;
}

void level_gen(struct level_t *level, int type)
{
    pthread_mutex_lock(&level->mutex);
    int r = pthread_create(&level->thread, NULL, &level_gen_thread, level);
}

bool level_load(const char *name, struct level_t **level)
{
    struct level_t *l;
    unsigned x, y, z;
    gzFile gz;

    char filename[64];
    snprintf(filename, sizeof filename, "levels/%s.mcl", name);

    gz = gzopen(filename, "rb");
    if (gz == NULL) return false;

    l = malloc(sizeof *l);
    gzread(gz, &x, sizeof x);
    gzread(gz, &y, sizeof y);
    gzread(gz, &z, sizeof z);
    level_init(l, x, y, z, name);

    gzread(gz, l->blocks, sizeof *l->blocks * x * y *z);

    gzclose(gz);

    level_list_add(&s_levels, l);

    printf("Loaded %s\n", filename);

    if (level != NULL) *level = l;
    return true;
}

bool level_save(struct level_t *l)
{
    char filename[64];
    snprintf(filename, sizeof filename, "levels/%s.mcl", l->name);

    gzFile gz = gzopen(filename, "wb");
    if (gz == NULL) return false;

    gzwrite(gz, &l->x, sizeof l->x);
    gzwrite(gz, &l->y, sizeof l->y);
    gzwrite(gz, &l->z, sizeof l->z);
    gzwrite(gz, l->blocks, sizeof *l->blocks * l->x * l->y * l->z);
    gzclose(gz);

    printf("Saved %s\n", filename);

    l->changed = false;

    return true;
}

bool level_get_by_name(const char *name, struct level_t **level)
{
    int i;
    struct level_t *l;

    for (i = 0; i < s_levels.used; i++)
    {
        l = s_levels.items[i];
        if (strcasecmp(l->name, name) == 0)
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
        if (s_levels.items[i]->changed)
        {
            level_save(s_levels.items[i]);
        }
    }
}
