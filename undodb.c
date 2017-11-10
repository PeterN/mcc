#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sqlite3.h>
#include "mcc.h"
#include "undodb.h"
#include "playerdb.h"
#include "util.h"

struct undodb_t
{
	sqlite3 *db;
	sqlite3_stmt *insert_stmt;
	sqlite3_stmt *query1_stmt;
	sqlite3_stmt *query2_stmt;
	sqlite3_stmt *query3_stmt;
};

struct undodb_t *undodb_init(const char *name)
{
	struct undodb_t u;

	char buf[256];
	snprintf(buf, sizeof buf, "undo/%s.db", name);
	lcase(buf);

	int res;
	res = sqlite3_open(buf, &u.db);
	if (res != SQLITE_OK)
	{
		LOG("Can't open database: %s\n", sqlite3_errmsg(u.db));
		sqlite3_close(u.db);
		return NULL;
	}

	char *err;
	sqlite3_exec(u.db, "CREATE TABLE IF NOT EXISTS undo (id INTEGER PRIMARY KEY AUTOINCREMENT, playerid INT, x INT, y INT, z INT, oldtype INT, olddata INT, newtype INT, time DATETIME)", NULL, NULL, &err);
	if (err != NULL)
	{
		LOG("Errrrr: %s\n", err);
		sqlite3_free(err);
	}

	res = sqlite3_prepare_v2(u.db, "INSERT INTO undo (playerid, x, y, z, oldtype, olddata, newtype, time) VALUES (?, ?, ?, ?, ?, ?, ?, ?)", -1, &u.insert_stmt, NULL);
	if (res != SQLITE_OK)
	{
		LOG("Can't prepare statement: %s\n", sqlite3_errmsg(u.db));
		sqlite3_close(u.db);
		return NULL;
	}

	res = sqlite3_prepare_v2(u.db, "SELECT playerid, time, COUNT(*) FROM undo GROUP BY playerid, time / 900 ORDER BY time DESC LIMIT 15", -1, &u.query1_stmt, NULL);
	if (res != SQLITE_OK)
	{
		LOG("Can't prepare statement: %s\n", sqlite3_errmsg(u.db));
		sqlite3_close(u.db);
		return NULL;
	}

	res = sqlite3_prepare_v2(u.db, "SELECT time, COUNT(*) FROM undo WHERE playerid = ? GROUP BY time / 900 ORDER BY time DESC LIMIT 30", -1, &u.query2_stmt, NULL);
	if (res != SQLITE_OK)
	{
		LOG("Can't prepare statement: %s\n", sqlite3_errmsg(u.db));
		sqlite3_close(u.db);
		return NULL;
	}

	res = sqlite3_prepare_v2(u.db, "SELECT x, y, z, oldtype, olddata, newtype FROM undo WHERE playerid = ? ORDER BY id DESC LIMIT ?", -1, &u.query3_stmt, NULL);
	if (res != SQLITE_OK)
	{
		LOG("Can't prepare statement: %s\n", sqlite3_errmsg(u.db));
		sqlite3_close(u.db);
		return NULL;
	}

	struct undodb_t *up = malloc(sizeof *up);
	*up = u;

	return up;
}

void undodb_close(struct undodb_t *u)
{
	if (u == NULL) return;

	sqlite3_finalize(u->insert_stmt);
	sqlite3_finalize(u->query1_stmt);
	sqlite3_finalize(u->query2_stmt);
	sqlite3_finalize(u->query3_stmt);
	sqlite3_close(u->db);
	free(u);
}

void undodb_log(struct undodb_t *u, int playerid, int16_t x, int16_t y, int16_t z, int oldtype, int olddata, int newtype)
{
	if (u == NULL) return;

	sqlite3_reset(u->insert_stmt);
	sqlite3_bind_int(u->insert_stmt, 1, playerid);
	sqlite3_bind_int(u->insert_stmt, 2, x);
	sqlite3_bind_int(u->insert_stmt, 3, y);
	sqlite3_bind_int(u->insert_stmt, 4, z);
	sqlite3_bind_int(u->insert_stmt, 5, oldtype);
	sqlite3_bind_int(u->insert_stmt, 6, olddata);
	sqlite3_bind_int(u->insert_stmt, 7, newtype);
	sqlite3_bind_int(u->insert_stmt, 8, time(NULL));

	int res = sqlite3_step(u->insert_stmt);
	if (res != SQLITE_DONE)
	{
		LOG("Undo log failed\n");
	}
}

void undodb_query(struct undodb_t *u, query_func_t func, void *arg)
{
	if (u == NULL) return;

	sqlite3_reset(u->query1_stmt);

	int res;
	while ((res = sqlite3_step(u->query1_stmt)) == SQLITE_ROW)
	{
		int playerid = sqlite3_column_int(u->query1_stmt, 0);
		time_t time = sqlite3_column_int(u->query1_stmt, 1);
		int count = sqlite3_column_int(u->query1_stmt, 2);

		char stime[64];
		strftime(stime, sizeof stime, "%H:%M", localtime(&time));
		char buf[64];
		snprintf(buf, sizeof buf, "%s (%d @ %s), ", playerdb_get_username(playerid), count, stime);
		func(buf, arg);
	}
}

void undodb_query_player(struct undodb_t *u, int playerid, query_func_t func, void *arg)
{
	if (u == NULL) return;

	sqlite3_reset(u->query2_stmt);
	sqlite3_bind_int(u->query2_stmt, 1, playerid);

	int res;
	while ((res = sqlite3_step(u->query2_stmt)) == SQLITE_ROW)
	{
		time_t time = sqlite3_column_int(u->query2_stmt, 0);
		int count = sqlite3_column_int(u->query2_stmt, 1);

		char stime[64];
		strftime(stime, sizeof stime, "%H:%M", localtime(&time));
		char buf[64];
		snprintf(buf, sizeof buf, "%d @ %s, ", count, stime);
		func(buf, arg);
	}
}

int undodb_undo_player(struct undodb_t *u, int playerid, int limit, undo_func_t func, void *arg)
{
	if (u == NULL) return 0;

	sqlite3_reset(u->query3_stmt);
	sqlite3_bind_int(u->query3_stmt, 1, playerid);
	sqlite3_bind_int(u->query3_stmt, 2, limit);

	int count = 0;
	int res;
	while ((res = sqlite3_step(u->query3_stmt)) == SQLITE_ROW)
	{
		int16_t x = sqlite3_column_int(u->query3_stmt, 0);
		int16_t y = sqlite3_column_int(u->query3_stmt, 1);
		int16_t z = sqlite3_column_int(u->query3_stmt, 2);
		int oldtype = sqlite3_column_int(u->query3_stmt, 3);
		int olddata = sqlite3_column_int(u->query3_stmt, 4);
		int newtype = sqlite3_column_int(u->query3_stmt, 5);

		count += func(x, y, z, oldtype, olddata, newtype, arg);
	}

	return count;
}
