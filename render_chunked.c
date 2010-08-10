#include <png.h>
#include "block.h"
#include "chunk.h"
#include "chunked_level.h"
#include "util.h"
#include "mcc.h"

struct server_t g_server;

enum
{
    ROT_0,
    ROT_90,
    ROT_180,
    ROT_270,
};

static const uint32_t s_colours[] =
{
    0x00000000,
    0xFF7D7D7D,
    0xFF75B049,
    0xFF866043,
    0xFF757575,
    0xFF9D804F,
    0x6C518117,
    0xFF545454,
    0x8A2A5EFF,
    0x8A2A5EFF,
    0xFFF54100,
    0xFFF54100,
    0xFFDAD29E,
    0xFF887E7E,
    0xFF8F8C7D,
    0xFF88827F,
    0xFF737373,
    0xFF665132,
    0x9A3CBF29,
    0xFFB6B639,
    0x47DaF1F4,
    0xFFDE3232,
    0xFFDE8832,
    0xFFDEDE32,
    0xFF88DE32,
    0xFF32DE32,
    0xFF32DE88,
    0xFF32DEDE,
    0xFF68A3DE,
    0xFF7878DE,
    0xFF8832DE,
    0xFFAE4ADE,
    0xFFDE32DE,
    0xFFDE3288,
    0xFF4D4D4D,
    0xFF8F8F8F,
    0xFFDEDEDE,
    0x1F6CA201,
    0x1E8B2C0D,
    0x1A8A6A54,
    0x22C33638,
    0xFFFFF144,
    0xFFE6E6E6,
    0xFF9F9F9F,
    0xFF9F9F9F,
    0xFFC66A50,
    0xFF82412F,
    0xFF6C583A,
    0xFF5B6C5B,
    0xFF14121E,
};

struct col_t {
    union {
            uint32_t argb;
            struct
            {
                uint8_t b;
                uint8_t g;
                uint8_t r;
                uint8_t a;
            };
    };
};

struct col_t blocktype_get_colour(enum blocktype_t block)
{
    struct col_t c;

    c.argb = s_colours[block < BLOCK_END ? block : ROCK];
    return c;
//    return c.argb & 0xFFFFFF;
//printf("%x : %x %x %x\n", c.argb, c.r, c.g, c.b);
//    return (c.b << 24) | (c.g << 16) | (c.r << 8) | c.a;
}

struct col_t darken(struct col_t c)
{
    c.r = c.r * 8 / 9;
    c.g = c.g * 8 / 9;
    c.b = c.b * 8 / 9;
    return c;
}

uint32_t col_to_uint32(struct col_t c)
{
    return (c.b << 24) | (c.g << 16) | (c.r << 8) | c.a;
}

void setpixel(uint32_t *map, int w, int h, int x, int y, struct col_t c)
{
    if (x < 0 || y < 0 || x >= w || y >= h)
    {
        printf("Pixel at %d x %d out of map (%d x %d)\n", x, y, w, h);
        return;
    }

    uint32_t *p = &map[x + y * w];
    if (c.a == 0x00) return;
    if (c.a == 0xFF)
    {
        *p = c.argb;
    }
    else
    {
        struct col_t oc;
        oc.argb = *p;
        oc.r = (c.r * c.a + oc.r * (0xFF - c.a)) / 0xFF;
        oc.g = (c.g * c.a + oc.g * (0xFF - c.a)) / 0xFF;
        oc.b = (c.b * c.a + oc.b * (0xFF - c.a)) / 0xFF;
        *p = oc.argb;
    }
}

enum blocktype_t level_get_blocktype(struct chunked_level_t *level, int16_t x, int16_t y, int16_t z)
{
    if (x < 0 || y < 0 || z < 0 || x >= level->size_x || y >= level->size_y || z >= level->size_z) return AIR;
    struct chunk_t *chunk = chunked_level_get_chunk(level, (x >> CHUNK_BITS_X) + level->min_x, (y >> CHUNK_BITS_Y) + level->min_y, (z >> CHUNK_BITS_Z) + level->min_z, true);

    if (!chunk->ready)
    {
        LOG("Waiting for chunk at %d x %d x %d\n", x >> CHUNK_BITS_X, y >> CHUNK_BITS_Y, z >> CHUNK_BITS_Z);
        while (!chunk->ready) usleep(100);
    }

    x &= CHUNK_MASK_X;
    y &= CHUNK_MASK_Y;
    z &= CHUNK_MASK_Z;

    const struct block_t *b = &chunk->blocks[x | ((y | z << CHUNK_BITS_Y) << CHUNK_BITS_X)];
    if (b->type == WATERSTILL) return WATER;
    return b->type;
}

static int16_t getshadowmap(struct chunked_level_t *level, int x, int z, int16_t *shadowmap)
{
    int16_t *smp = &shadowmap[x + z * level->size_x];
    if (*smp == INT16_MIN)
    {
        int y;
        for (y = level->size_y - 1; y >= 0; y--)
        {
            if (level_get_blocktype(level, x, y, z) != AIR)
            {
                *smp = y;
                break;
            }
        }
    }

    return *smp;
}

#define SCALEU 2
#define SCALED 1

uint32_t *level_render_iso(struct chunked_level_t *level, int rot, int *w, int *h)
{
    int size_x = level->size_x;
    int size_y = level->size_y;
    int size_z = level->size_z;

    *w = (size_x + size_z) * SCALEU / SCALED + 4;
    *h = (size_x + size_z) * SCALEU / SCALED / 2 + size_y * SCALEU / SCALED + 4;

    int size = *w * *h * sizeof (uint32_t);
    uint32_t *map = malloc(size);

    memset(map, 0, size);

    int16_t *shadowmap = malloc(size_x * size_z * sizeof *shadowmap);

    int x, y, z;

    /* Initialize shadow map */
    for (z = 0; z < size_z; z++)
    {
        for (x = 0; x < size_x; x++)
        {
            shadowmap[x + z * size_x] = INT16_MIN;
        }
    }


    int mx, my;
    int ox = -size_z * SCALEU / SCALED;
    int oy = size_y * SCALEU / SCALED;

    for (z = 0; z < size_z; z++)
    {
        for (x = 0; x < size_x; x++)
        {
            if ((x & CHUNK_MASK_X) == 0 && (z & CHUNK_MASK_Z) == 0)
            {
                /* Preload chunks */
//                LOG("Preloading %d %d %d\n", (x >> CHUNK_BITS_X) + level->min_x, level->min_y, (z >> CHUNK_BITS_Z) + level->min_z);
                chunked_level_get_chunk(level, (x >> CHUNK_BITS_X) + level->min_x, level->min_y, (z >> CHUNK_BITS_Z) + level->min_z, true);
    //            LOG("Preloading %d %d %d\n", (x >> CHUNK_BITS_X) + level->min_x + 1, level->min_y, (z >> CHUNK_BITS_Z) + level->min_z);
//
                chunked_level_get_chunk(level, (x >> CHUNK_BITS_X) + level->min_x + 1, level->min_y, (z >> CHUNK_BITS_Z) + level->min_z, true);
                chunked_level_get_chunk(level, (x >> CHUNK_BITS_X) + level->min_x + 2, level->min_y, (z >> CHUNK_BITS_Z) + level->min_z, true);

    //            LOG("Preloading %d %d %d\n", (x >> CHUNK_BITS_X) + level->min_x, level->min_y, (z >> CHUNK_BITS_Z) + level->min_z + 1);

                chunked_level_get_chunk(level, (x >> CHUNK_BITS_X) + level->min_x, level->min_y, (z >> CHUNK_BITS_Z) + level->min_z + 1, true);


                if (x > 0)
                {
                    struct chunk_t *chunk = chunked_level_get_chunk(level, (x >> CHUNK_BITS_X) + level->min_x - 1, (y >> CHUNK_BITS_Y) + level->min_y, (z >> CHUNK_BITS_Z) + level->min_z, false);
                    if (chunk != NULL) chunk->inuse = false;
                }
            }

            int cy = getshadowmap(level, x, z, shadowmap);
            int cy1 = (z < size_z - 1) ? getshadowmap(level, x, z + 1, shadowmap) : 0;
            int cy2 = (x < size_x - 1) ? getshadowmap(level, x + 1, z, shadowmap) : 0;

            for (y = 0; y < size_y; y++)
            {
                enum blocktype_t b = level_get_blocktype(level, x, y, z);
                struct col_t c = blocktype_get_colour(b);

                if (b == AIR) continue;

                bool trans = false;
                if (b == WATER || b == GLASS) trans = true;

                mx = (x - z) * SCALEU / SCALED - ox;
                my = (z + x) * SCALEU / SCALED / 2 - y * SCALEU / SCALED + oy;

                if (!trans || level_get_blocktype(level, x, y + 1, z) != b)
                {
                    if (y < cy)
                    {
                        c = darken(c);
                        c = darken(c);
                    }

                    int sy, sx;
                    for (sy = -SCALEU; sy < SCALEU; sy += 2)
                    for (sx = -SCALEU + abs(sy); sx < SCALEU - abs(sy); sx++)
                    {
                        setpixel(map, *w, *h, mx + sx, my + sy / 2, c);
                    }
                    /*setpixel(map, *w, *h, mx + 0, my, c);
                    setpixel(map, *w, *h, mx + 1, my, c);
                    setpixel(map, *w, *h, mx + 2, my, c);
                    setpixel(map, *w, *h, mx + 3, my, c);*/
                    if (y < cy)
                    {
                        c = blocktype_get_colour(b);
                    }
                }

                if (b == GRASS) c = blocktype_get_colour(DIRT);
                struct col_t c1 = darken(c);
                struct col_t c2 = darken(c1);

                if (y < cy1)
                {
                    c1 = darken(c1);
                    c1 = darken(c1);
                }
                if (y < cy2)
                {
                    c2 = darken(c2);
                    c2 = darken(c2);
                }

                if (!trans || level_get_blocktype(level, x, y, z + 1) != b)
                {
                    int sx, sy;
                    for (sy = 0; sy < SCALEU; sy++)
                    for (sx = 0; sx < SCALEU; sx++)
                    setpixel(map, *w, *h, mx - SCALEU + sx, my + sy + sx / 2 + 1, c1);
                /*
                    setpixel(map, *w, *h, mx + 0, my + 1, c1);
                    setpixel(map, *w, *h, mx + 1, my + 1, c1);
                    setpixel(map, *w, *h, mx + 0, my + 2, c1);
                    setpixel(map, *w, *h, mx + 1, my + 2, c1);
                    if (!trans) setpixel(map, *w, *h, mx + 1, my + 3, c1);*/
                }

                if (!trans || level_get_blocktype(level, x + 1, y, z) != b)
                {
                    int sx, sy;
                    for (sy = 0; sy < SCALEU; sy++)
                    for (sx = 0; sx < SCALEU; sx++)
                    setpixel(map, *w, *h, mx + SCALEU - 1 - sx, my + sy + sx / 2 + 1, c2);
/* 
                    setpixel(map, *w, *h, mx + 2, my + 1, c2);
                    setpixel(map, *w, *h, mx + 3, my + 1, c2);
                    setpixel(map, *w, *h, mx + 2, my + 2, c2);
                    setpixel(map, *w, *h, mx + 3, my + 2, c2);
                    if (!trans) setpixel(map, *w, *h, mx + 2, my + 3, c2);*/
                }
            }
        }
    }

    /* Convert from ARGB to format required for PNG */
    int i;
    for (i = 0; i < size / sizeof (uint32_t); i++)
    {
        struct col_t c;
        c.argb = map[i];
        map[i] = col_to_uint32(c);
    }

    return map;
}


uint32_t *level_render_flat(struct chunked_level_t *level, int rot, int *w, int *h)
{
    int size_x = level->size_x;
    int size_y = level->size_y;
    int size_z = level->size_z;

    int row, col, off;
    switch (rot)
    {
        case ROT_0:
            *w = size_x;
            *h = size_z;
            row = *w;
            col = 1;
            off = 0;
            break;

        case ROT_90:
            *w = size_z;
            *h = size_x;
            row = 1;
            col = -*w;
            off = *w * (*h - 1);
            break;

        case ROT_180:
            *w = size_x;
            *h = size_z;
            row = -*w;
            col = -1;
            off = (*w * *h) - 1;
            break;

        case ROT_270:
            *w = size_z;
            *h = size_x;
            row = -1;
            col = *w;
            off = *w - 1;
            break;
    }

    uint32_t *map = malloc(size_x * size_z * sizeof *map);

    int x, y, z;
    for (z = 0; z < size_z; z++)
    {
        int i = off + z * row;
        for (x = 0; x < size_x; x++)
        {
            int water = 0;
            enum blocktype_t block = AIR;
            for (y = size_y - 1; y >= 0; y--)
            {
                block = level_get_blocktype(level, x, y, z);

                if (block == AIR) continue;
                if (block == WATER || block == WATERSTILL)
                {
                    water++;
                    continue;
                }
                break;
            }

            struct col_t c = blocktype_get_colour(block);
            struct col_t w = blocktype_get_colour(WATER);
            for (y = 0; y < water; y++)
            {
                //printf("y %d, %x %x %x - %x %x %x %x\n", y, c.r, c.g, c.b, w.r, w.g, w.b, w.a);
                c.r = (w.r * w.a + c.r * (0xFF - w.a)) / 0xFF;
                c.g = (w.g * w.a + c.g * (0xFF - w.a)) / 0xFF;
                c.b = (w.b * w.a + c.b * (0xFF - w.a)) / 0xFF;
            }

            map[i] = col_to_uint32(c);
            i += col;
        }
    }

    return map;
}

uint32_t *level_render(struct chunked_level_t *level, int rot, bool iso, int *w, int *h)
{
    //if (iso) return level_render_iso(level, rot, w, h);

    return level_render_iso(level, rot, w, h);
}

static void PNGAPI png_my_error(png_structp png_ptr, png_const_charp message)
{
    fprintf(stderr, "libpng error: %s - %s\n", message, (const char *)png_get_error_ptr(png_ptr));
    longjmp(png_jmpbuf(png_ptr), 1);
}

static void PNGAPI png_my_warning(png_structp png_ptr, png_const_charp message)
{
    fprintf(stderr, "libpng warning: %s - %s\n", message, (const char *)png_get_error_ptr(png_ptr));
}

void level_render_png(struct chunked_level_t *level, int rot, bool iso)
{
    char filename[256];
    png_structp png_ptr;
    png_infop info_ptr;

    snprintf(filename, sizeof filename, "%s.png", level->name);
    lcase(filename);

    FILE *f = fopen(filename, "wb");
    if (f == NULL) return;

    int w, h;
    uint32_t *imagebuf = level_render(level, rot, iso, &w, &h);

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (void *)filename, png_my_error, png_my_warning);
    if (png_ptr == NULL)
    {
        fclose(f);
        return;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL)
    {
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(f);
        return;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(f);
        return;
    }

    png_init_io(png_ptr, f);
    png_set_filter(png_ptr, 0, PNG_FILTER_NONE);
    png_set_IHDR(png_ptr, info_ptr, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);
    png_set_flush(png_ptr, 512);

    png_color_8 sig_bit;
    sig_bit.alpha = 8;
    sig_bit.blue  = 8;
    sig_bit.green = 8;
    sig_bit.red   = 8;
    sig_bit.gray  = 0;
    png_set_sBIT(png_ptr, info_ptr, &sig_bit);
    png_set_filler(png_ptr, 0, PNG_FILLER_BEFORE);

    int y;
    for (y = 0; y < h; y++)
    {
        png_write_row(png_ptr, (png_bytep)imagebuf + y * w * 4);
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    free(imagebuf);
    fclose(f);
}

int main(int argc, char **argv)
{
    g_server.logfile = stderr;

    if (argc != 7) return 0;

    int x = atoi(argv[1]);
    int y = atoi(argv[2]);
    int z = atoi(argv[3]);
    int rx = atoi(argv[4]);
    int ry = atoi(argv[5]);
    int rz = atoi(argv[6]);

    rand();

    struct chunked_level_t *l = malloc(sizeof *l);
    chunked_level_init(l, "test");
    chunked_level_set_area(l, x, y, z, rx, ry, rz);

    LOG("Map size is %d %d %d\n", l->size_x, l->size_y, l->size_z);

    level_render_png(l, ROT_0, false);

    chunked_level_hash_analysis(l);

    return 0;
}
