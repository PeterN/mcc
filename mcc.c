#include <stdio.h>
#include <unistd.h>
#include "block.h"
#include "mcc.h"
#include "level.h"
#include "network.h"
#include "player.h"
#include "playerdb.h"
#include "client.h"
#include "heartbeat.h"
#include "irc.h"

struct server_t g_server;

int main(int argc, char **argv)
{
    int tick = 0;
	int i;

    if (!level_load("main", NULL))
    {
        struct level_t *l = malloc(sizeof *l);
        level_init(l, 128, 32, 128, "main");
        level_gen(l, 1);
        level_list_add(&s_levels, l);
    }

	g_server.name = "TEST TEST TEST";
	g_server.motd = "Test server for Just Another Minecraft Server";
	g_server.max_players = 20;
	g_server.players = 0;
	g_server.public = true;
	g_server.exit = false;

	g_server.irc.hostname = "irc.lspace.org";
	g_server.irc.port = 6667;
	g_server.irc.name = "mccbot";
	g_server.irc.channel = "#mc2";
	g_server.irc.pass = NULL;

	playerdb_init();

	net_init();
	//irc_start();

	while (!g_server.exit)
	{
		usleep(1000);
		tick++;
		net_run();

		if ((tick % 2500) == 0) client_info();
		if ((tick % 60000) == 0) heartbeat_start();
		if ((tick % 20000) == 0) level_save_all();
		if ((tick % 20000) == 0) level_unload_empty();

		if ((tick % 33) == 0) player_send_positions();

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

	playerdb_close();

	return 0;
}
