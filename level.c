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

struct level_list_t s_levels;

bool level_t_compare(struct level_t **a, struct level_t **b)
{
    return *a == *b;
}

void level_init(struct level_t *level, unsigned x, unsigned y, unsigned z)
{
    memset(level, 0, sizeof *level);

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
    //if (deflateInit(&_z, 5) != Z_OK) return;

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
    }
    while (z.avail_in > 0 || z.avail_out == 0);

    deflateEnd(&z);

    client_add_packet(c, packet_send_level_finalize(level->x, level->y, level->z));

    //client_add_packet(c, packet_send_teleport_player(0, 32*32, 2048*32, 32*32, 0, 0));

    //struct packet_t *packet_send_level_initialize();
    //struct packet_t *packet_send_level_data_chunk(int16_t chunk_length, uint8_t *data, uint8_t percent);
    //struct packet_t *packet_send_level_finalize(int16_t x, int16_t y, int16_t z);

    c->waiting_for_level = false;

    return true;
}

static const float _amplitudes[] = {
    16000, 19200, 12800, 8000, 3200, 256, 64
};

float random_height(int max)
{
    return ((float)rand() / INT_MAX - 0.5f) * max;
}

bool level_apply_noise(float *map, int mx, int mz, int log_frequency, int amplitude)
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

void level_normalize(float *map, int mx, int mz, int my)
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

    printf("Min: %f, Max: %f\n", hmin, hmax);

    for (z = 0; z <= mz; z++)
    {
        for (x = 0; x <= mx; x++)
        {
            float h = map[x + z * dmx];
            h -= hmin;
            h /= (hmax - hmin);
            h *= my / 8;
            h += my / 2;
            map[x + z * dmx] = h < 0 ? 0 : h >= my ? my - 1 : h;
        }
    }
}


void *level_gen_thread(void *arg)
{
    struct level_t *level = arg;
    struct block_t block;
    int i, x, y, z;
    int mx = level->x;
    int my = level->y;
    int mz = level->z;

    float *hm = malloc((mx + 1) * (mz + 1) * sizeof *hm);

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

        continue_iteration = level_apply_noise(hm, mx, mz, iteration_round, amplitude);
        iteration_round++;
    } while (continue_iteration);

    level_normalize(hm, mx, mz, my);
    int dmx = mx + 1;

    for (z = 0; z < mz; z++)
    {
        for (x = 0; x < mx; x++)
        {
            int h = hm[x + z * dmx];
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

    pthread_mutex_unlock(&level->mutex);

    return NULL;
}

void level_gen(struct level_t *level, int type)
{
    pthread_mutex_lock(&level->mutex);
    int r = pthread_create(&level->thread, NULL, &level_gen_thread, level);
}
