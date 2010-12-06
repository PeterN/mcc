#ifndef RANK_H
#define RANK_H

enum rank_t
{
	RANK_BANNED      = 0,
	RANK_GUEST       = 10,
	RANK_REGULAR     = 30,
	RANK_BUILDER     = 40,
	RANK_ADV_BUILDER = 50,
	RANK_MOD         = 70,
	RANK_OP          = 80,
	RANK_ADMIN       = 100,
};

static inline enum rank_t rank_convert(int rank)
{
	switch (rank)
	{
		case 0: return RANK_BANNED;
		case 1: return RANK_GUEST;
		case 2: return RANK_BUILDER;
		case 3: return RANK_ADV_BUILDER;
		case 4: return RANK_OP;
		case 5: return RANK_ADMIN;
		default:
			return RANK_GUEST;
	}
}

#endif /* RANK_H */
