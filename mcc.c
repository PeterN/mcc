#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include "block.h"
#include "config.h"
#include "mcc.h"
#include "level.h"
#include "level_worker.h"
#include "astar_worker.h"
#include "module.h"
#include "network.h"
#include "player.h"
#include "playerdb.h"
#include "client.h"
#include "timer.h"

struct server_t g_server;

static unsigned gettime(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void mcc_exit(void)
{
	level_save_all(NULL);

	level_worker_deinit();
	astar_worker_deinit();

	unsigned i;
	for (i = 0; i < s_levels.used; i++)
	{
		struct level_t *l = s_levels.items[i];
		if (l != NULL)
		{
			level_unload(l);
			s_levels.items[i] = NULL;
		}
	}

	modules_deinit();
	level_list_free(&s_levels);
	level_hooks_deinit();
	timers_deinit();
	blocktype_deinit();
	socket_deinit();

	playerdb_close();

	config_deinit();

	LOG("Server exiting...\n");

	fclose(g_server.logfile);
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

	g_server.logfile = fopen("log.txt", "a");
	LOG("Server starting...\n");

	config_init("config.txt");

	if (!config_get_string("name", &g_server.name) ||
		!config_get_string("motd", &g_server.motd) ||
		!config_get_int("max_players", &g_server.max_players) ||
		!config_get_int("port", &g_server.port) ||
		!config_get_int("public", &g_server.public))
	{
		LOG("Couldn't read required config parameters\n");
		return 0;
	}

	level_worker_init();
	astar_worker_init();

	g_server.players = 0;
	g_server.exit = false;
	g_server.start_time = time(NULL);
	g_server.cpu_start = clock();
	g_server.pos_interval = 40;
	g_server.cuboid_max = 100;

	signal(SIGINT, &sighandler);

	blocktype_init();

	playerdb_init();

	modules_init();

	register_timer("save levels", 120000, &level_save_all, NULL);
	register_timer("unload levels", 20000, &level_unload_empty, NULL);

	if (!level_load("main", NULL))
	{
		struct level_t *l = malloc(sizeof *l);
		if (!level_init(l, 128, 32, 128, "main", true))
		{
			LOG("Unable to create main level, exiting.\n");
			free(l);
			return false;
		}

		level_gen(l, "adminium", l->y / 2, l->y / 2);

		l->rankbuild = RANK_GUEST;
		l->rankvisit = RANK_GUEST;
		l->rankown   = RANK_OP;

		level_list_add(&s_levels, l);
	}

	net_init(g_server.port);

	unsigned cur_ticks = gettime();
	unsigned next_tick = cur_ticks + TICK_INTERVAL;

	while (!g_server.exit)
	{
		unsigned prev_cur_ticks = cur_ticks;

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

			process_timers(cur_ticks);

			level_process_physics((tick % MS_TO_TICKS(80)) == 0);
			level_process_updates(true);

			cuboid_process();
			if ((tick % MS_TO_TICKS(g_server.pos_interval)) == 0)
			{
				player_send_positions();
				npc_send_positions();
			}
		}

		usleep(50);
	}

	mcc_exit();

	return 0;
}
