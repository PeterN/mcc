#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "faultgen.h"
#include "mcc.h"

struct faultgen_t
{
	int x, y;
	float map[];
};

struct faultgen_t *faultgen_init(int x, int y)
{
	struct faultgen_t *f = malloc(sizeof *f + sizeof *f->map * x * y);
	if (f == NULL)
	{
		LOG("[faultgen] faultgen_init(): couldn't allocate %zu bytes\n", sizeof *f + sizeof *f->map * x * y);
		return NULL;
	}

	f->x = x;
	f->y = y;

	return f;
}

void faultgen_deinit(struct faultgen_t *f)
{
	free(f);
}

void faultgen_create(struct faultgen_t *f, bool mountains)
{
	float disp_max;
	float disp_delta;
	float start_height;

	if (mountains)
	{
		disp_max = 0.02f;
		disp_delta = -0.0025f;
		start_height = 0.6f;
	}
	else
	{
		disp_max = 0.05f;
		disp_delta = -0.005f;
		start_height = 0.5f;
	}

	int hx = f->x / 2;
	int hy = f->y / 2;
	int hx2 = hx * hx;
	int hy2 = hy * hy;
	float d = sqrt(hx2 + hy2);

	float disp_min = -disp_max;
	float disp = disp_max;

	int i;

	LOG("faultgen: generating %d by %d map\n", f->x, f->y);

	for (i = 0; i < f->x * f->y; i++)
	{
		f->map[i] = start_height;
	}

	LOG("faultgen: starting %d iterations\n", f->x + f->y);

	for (i = 0; i < f->x + f->y; i++)
	{
//		float w = (rand() % 3600) / 10.0;
		float w = ((float)rand() / RAND_MAX) * 2.0 * M_PI;
		float a = cos(w);
		float b = sin(w);
		float c = ((float)rand() / RAND_MAX) * 2.0 * d - d;

		int x, y;
		for (x = 0; x < f->x; x++)
		{
			for (y = 0; y < f->y; y++)
			{
				float h = ((y - hy) * a + (x - hx) * b + c > 0) ? disp : -disp;
				float *m = &f->map[x + y * f->x];

				*m += h;
				//if (*m > 1) *m = 1;
				//if (*m < 0) *m = 0;
			}
		}

		disp += disp_delta;
		if (disp < disp_min) disp = disp_max;
	}

	LOG("faultgen: normalizing\n");

	float min = +INFINITY;
	float max = -INFINITY;
	for (i = 0; i < f->x * f->y; i++)
	{
		if (f->map[i] < min) min = f->map[i];
		if (f->map[i] > max) max = f->map[i];
	}

	for (i = 0; i < f->x * f->y; i++)
	{
		f->map[i] = (f->map[i] - min) / (max - min);
	}

	LOG("faultgen: complete\n");
}

const float *faultgen_map(struct faultgen_t *f)
{
	return f->map;
}
