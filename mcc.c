#include <stdio.h>
#include <unistd.h>
#include "mcc.h"
#include "level.h"
#include "network.h"
#include "player.h"

int main(int argc, char **argv)
{
    int tick = 0;
	struct level_t level;

	level_init(&level, 64, 64, 64);
	level.name = strdup("main");

	level_clear_block(&level, level_get_index(&level, 1, 1, 1));

	level_list_add(&s_levels, &level);

	net_init();

	while (1)
	{
		usleep(1000);
		tick++;
		net_run();

		if ((tick % 5000) == 0) player_info();
		if ((tick % 60000) == 0) heartbeat_start();
	}

	return 0;
}
