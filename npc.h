#include <stdint.h>

struct npc
{
	int levelid;
	char name[64];

	struct level_t *level;

	struct position_t pos;
	struct position_t oldpos;
	struct position_t target;
};
