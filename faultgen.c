#include <stdlib.h>
#include <math.h>
#include "faultgen.h"

struct faultgen_t
{
	int x, y;
	float disp_max;
	float disp_delta;
	float map[];
};

struct faultgen_t *faultgen_init(int x, int y)
{
	struct faultgen_t *f = malloc(sizeof *f + sizeof *f->map * x * y);
	f->x = x;
	f->y = y;
	f->disp_max = 0.01f;
	f->disp_delta = -0.0025f;

	return f;
}

void faultgen_deinit(struct faultgen_t *f)
{
	free(f);
}

void faultgen_create(struct faultgen_t *f)
{
	int hx = f->x / 2;
	int hy = f->y / 2;
	int hx2 = hx * hx;
	int hy2 = hy * hy;
	float d = sqrt(hx2 + hy2);

	float disp_min = -f->disp_max;
	float disp = f->disp_max;

	int i;

	for (i = 0; i < f->x * f->y; i++)
	{
		f->map[i] = 0.5;
	}

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
				if (*m > 1) *m = 1;
				if (*m < 0) *m = 0;
			}
		}

		disp += f->disp_delta;
		if (disp < disp_min) disp = f->disp_max;
	}
}

const float *faultgen_map(struct faultgen_t *f)
{
	return f->map;
}
