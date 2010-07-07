#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "level.h"
#include "block.h"
#include "client.h"
#include "packet.h"

struct level_list_t s_levels;

bool level_t_compare(struct level_t **a, struct level_t **b)
{
    return *a == *b;
}

void level_init(struct level_t *level, unsigned x, unsigned y, unsigned z)
{
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

void level_send(struct client_t *c, unsigned index)
{
    struct level_t *level = s_levels.items[index];
    unsigned length = level->x * level->y * level->z;
    int x;
    z_stream z;

    memset(&z, 0, sizeof z);
    if (deflateInit2(&z, 5, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK) return;
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

    client_add_packet(c, packet_send_teleport_player(0, 32*32, 2048*32, 32*32, 0, 0));

    //struct packet_t *packet_send_level_initialize();
    //struct packet_t *packet_send_level_data_chunk(int16_t chunk_length, uint8_t *data, uint8_t percent);
    //struct packet_t *packet_send_level_finalize(int16_t x, int16_t y, int16_t z);
}

void level_gen(struct level_t *level, int type)
{
    struct block_t block;
    int x, y, z;
    int mx = level->x;
    int my = level->y;
    int mz = level->z;

    int i;
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
    }
}
