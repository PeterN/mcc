#include <stdio.h>
#include <unistd.h>
#include "block.h"
#include "mcc.h"
#include "level.h"
#include "network.h"
#include "player.h"

struct server_t g_server;

int main(int argc, char **argv)
{
    int tick = 0;
	struct level_t level;
	int x, y, z;

	level_init(&level, 64, 64, 64);
	level.name = strdup("main");

	level_gen(&level, 0);

	level_list_add(&s_levels, &level);

	g_server.name = "TEST TEST TEST";
	g_server.motd = "This is a test";
	g_server.max_players = 10;
	g_server.players = 0;
	g_server.public = false;

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
