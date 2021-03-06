#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "block.h"
#include "config.h"
#include "commands.h"
#include "mcc.h"
#include "level.h"
#include "level_worker.h"
#include "astar_worker.h"
#include "module.h"
#include "network.h"
#include "network_worker.h"
#include "player.h"
#include "playerdb.h"
#include "client.h"
#include "socket.h"
#include "timer.h"
#include "gettime.h"

struct server_t g_server;

void mcc_exit(void)
{
	net_deinit();

	physics_deinit();
	level_save_all(NULL);

	level_worker_deinit();
	astar_worker_deinit();
	network_worker_deinit();

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
	commands_deinit();

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

static void generate_salt(void *arg)
{
	/* Generate salt */
	static const char saltchars[] = "0123456789abcdef";

	char salt[33];
	int len = 32;

	int i;
	for (i = 0; i < len; i++)
	{
		salt[i] = saltchars[rand() % (sizeof saltchars - 1)];
	}
	salt[i] = '\0';

	if (g_server.salt != NULL)
	{
		free(g_server.old_salt);
		g_server.old_salt = strdup(g_server.salt);
	}

	config_set_string("salt", salt);
	config_write();
}

static void update_positions(void *arg)
{
	player_send_positions(gettime() / 1000);
	npc_send_positions();
}

static void update_cputime(void *arg)
{
	clock_t c = clock();
	g_server.cpu_time = ((double) (c - g_server.cpu_start)) / CLOCKS_PER_SEC * 100.0;
	g_server.cpu_start = c;
}

int main(int argc, char **argv)
{
	srand(getpid() + time(NULL));

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

	if (!config_get_string("salt", &g_server.salt))
	{
		generate_salt(NULL);
		config_get_string("salt", &g_server.salt);
	}

	if (!config_get_int("usleep", &g_server.usleep)) g_server.usleep = 50;
	if (!config_get_int("physics_usleep", &g_server.physics_usleep)) g_server.physics_usleep = 1000;

	level_worker_init();
	astar_worker_init();
	network_worker_init();

	commands_init();

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

	register_timer("save levels", 120000, &level_save_all, NULL, true);
	register_timer("unload levels", 20000, &level_unload_empty, NULL, true);
	register_timer("salt", 15 * 60 * 1000, &generate_salt, NULL, false);
	register_timer("positions", g_server.pos_interval, &update_positions, NULL, true);
	register_timer("cputime", 1000, &update_cputime, NULL, true);

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
	physics_init();

	while (!g_server.exit)
	{
		net_run();
		socket_run();
		process_timers(gettime());
		usleep(g_server.usleep);
	}

	mcc_exit();

	return 0;
}
