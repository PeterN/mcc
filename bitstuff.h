#ifndef BITSTUFF_H
#define BITSTUFF_H

#include <stdint.h>
#include <stdbool.h>

#define HasBit(x, y) (((x) & (1U << (y))) != 0)
//static inline bool HasBit(unsigned x, unsigned y)
//{
//	return (x & (1U << y)) != 0;
//}

#define SetBit(x, y) (x) |= (1U << (y))
//static inline void SetBit(unsigned x, unsigned y)
//{
    //x |= (1U << y);
//}

#define ToggleBit(x, y) (x) ^= (1U << (y))
//static inline void ToggleBit(unsigned x, unsigned y)
//{
//  x ^= (1U << y);
//}

static inline unsigned GetBits(unsigned x, unsigned s, unsigned n)
{
	return (x >> s) & ((1U << n) - 1);
}

#endif /* BITSTUFF_H */
