#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include "block.h"
#include "mcc.h"
#include "level.h"
#include "network.h"
#include "player.h"
#include "playerdb.h"
#include "client.h"

struct server_t g_server;

static int gettime()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void mcc_exit()
{
	level_save_all(NULL);

	/* Wait for threads to finish */
	unsigned i;
	for (i = 0; i < s_levels.used; i++)
	{
		struct level_t *l = s_levels.items[i];
		if (l != NULL && l->thread_valid)
		{
			pthread_join(l->thread, NULL);
			l->thread_valid = false;
		}
	}

	playerdb_close();

	LOG("Server exiting...\n");
}

static void sighandler(int sig)
{
	mcc_exit();

	exit(0);
}

#define TICK_INTERVAL 40
#define MS_TO_TICKS(x) ((x) / TICK_INTERVAL)

int main(int argc, char **argv)
{
	int tick = 0;
	unsigned i;

	//config_read(&g_server);

	g_server.logfile = fopen("log.txt", "a");
	LOG("Server starting...\n");

	if (!level_load("main", NULL))
	{
		struct level_t *l = malloc(sizeof *l);
		if (!level_init(l, 128, 32, 128, "main", true))
		{
			LOG("Unable to create main level, exiting.\n");
			free(l);
			return false;
		}

		level_gen(l, 1);

		l->rankbuild = RANK_GUEST;
		l->rankvisit = RANK_GUEST;

		level_list_add(&s_levels, l);
	}

	g_server.name = "TEST TEST TEST";
	g_server.motd = "Test server for Just Another Minecraft Server";
	g_server.max_players = 20;
	g_server.players = 0;
	g_server.public = true;
	g_server.exit = false;
	g_server.start_time = time(NULL);
	g_server.cpu_start = clock();

	g_server.irc.hostname = "irc.lspace.org";
	g_server.irc.port = 6667;
	g_server.irc.name = "mccbot";
	g_server.irc.channel = "#mc2";
	g_server.irc.pass = NULL;

	signal(SIGINT, &sighandler);

	blocktype_init();

	playerdb_init();

	net_init();

	module_load("heartbeat.so");
	module_load("irc.so");
	//irc_start();

	register_timer(120000, &level_save_all, NULL);
	register_timer(20000, &level_unload_empty, NULL);

	int cur_ticks = gettime();
	int next_tick = cur_ticks + TICK_INTERVAL;

	while (!g_server.exit)
	{
		int prev_cur_ticks = cur_ticks;

		net_run();

		cur_ticks = gettime();
		if (cur_ticks >= next_tick || cur_ticks < prev_cur_ticks)
		{
			next_tick = cur_ticks + TICK_INTERVAL;

			tick++;

			if ((tick % MS_TO_TICKS(1000)) == 0)
			{
				clock_t c = clock();
				g_server.cpu_time = ((double) (c - g_server.cpu_start)) / CLOCKS_PER_SEC * 100.0;
				g_server.cpu_start = c;
			}
			//if ((tick % MS_TO_TICKS(2500)) == 0) client_info();
			//if ((tick % MS_TO_TICKS(60000)) == 0) heartbeat_start();
			//if ((tick % MS_TO_TICKS(120000)) == 0) level_save_all();
			//if ((tick % MS_TO_TICKS(20000)) == 0) level_unload_empty();

			timer_process();

			//if ((tick % MS_TO_TICKS(240)) == 0)

			level_process_physics((tick % MS_TO_TICKS(80)) == 0);
			level_process_updates(true);
			//(tick % MS_TO_TICKS(240)) == 0);

			cuboid_process();
			player_send_positions();

			//if ((tick % MS_TO_TICKS(1000)) == 0)
			{
				for (i = 0; i < s_clients.used; i++)
				{
					struct client_t *c = s_clients.items[i];
					if (c->player != NULL && c->player->new_level != c->player->level)
					{
						level_send(c);
					}
				}
			}
		}

		usleep(1000);
	}

	mcc_exit();

	return 0;
}
