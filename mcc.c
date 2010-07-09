#include <stdio.h>
#include <unistd.h>
#include "block.h"
#include "mcc.h"
#include "level.h"
#include "network.h"
#include "player.h"
#include "client.h"

struct server_t g_server;

int main(int argc, char **argv)
{
    int tick = 0;
	int i;

    if (!level_load("main"))
    {
        struct level_t *l = malloc(sizeof *l);
        level_init(l, 256, 64, 256, "main");
        level_gen(l, 0);
        level_list_add(&s_levels, l);
    }

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
		if ((tick % 20000) == 0) level_save_all();

        if ((tick % 1000) == 0)
        {
            for (i = 0; i < s_clients.used; i++)
            {
                struct client_t *c = &s_clients.items[i];
                if (c->waiting_for_level)
                {
                    level_send(c);
                }
            }
        }
	}

	level_save_all();

	return 0;
}
