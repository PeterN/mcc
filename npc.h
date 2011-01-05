#ifndef NPC_H
#define NPC_H

#include <stdint.h>

struct npc
{
	struct level_t *level;
	int levelid;

	char name[64];

	struct position_t pos;
	struct position_t oldpos;
};

struct npc *npc_add(struct level_t *level, const char *name, struct position_t position);
void npc_del(struct npc *npc);
void npc_send_positions(void);

#endif /* NPC_H */
