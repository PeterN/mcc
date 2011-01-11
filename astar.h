#ifndef ASTAR_H
#define ASTAR_H

struct level_t;

struct point
{
	int x, y, z;
};

struct point *as_find(const struct level_t *level, const struct point *a, const struct point *b);

#endif /* ASTAR_H */
