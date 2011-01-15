#ifndef GETTIME_H
#define GETTIME_H

#include <time.h>

static inline unsigned gettime(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

#endif /* GETTIME_H */
