#include <stdio.h>
#include <time.h>
#include <sqlite3.h>
#include "mcc.h"
#include "player.h"

static sqlite3 *s_db;
static sqlite3_stmt *s_rank_get_stmt;
static sqlite3_stmt *s_rank_set_stmt;
static sqlite3_stmt *s_new_user_stmt;
static sqlite3_stmt *s_globalid_get_stmt;
static sqlite3_stmt *s_username_get_stmt;
static sqlite3_stmt *s_password_stmt;

void playerdb_init()
{
    int res;

    res = sqlite3_open("player.db", &s_db);
    if (res != SQLITE_OK)
    {
        LOG("Can't open database: %s", sqlite3_errmsg(s_db));
        sqlite3_close(s_db);
        return;
    }

    char *err;
    sqlite3_exec(s_db, "CREATE TABLE IF NOT EXISTS players (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT UNIQUE, rank INT, password TEXT, first_visit DATETIME, last_visit DATETIME)", NULL, NULL, &err);
    if (err != NULL)
    {
        LOG("Errrrr: %s", err);
        sqlite3_free(err);
    }

    sqlite3_exec(s_db, "CREATE TABLE IF NOT EXISTS bans (net TEXT PRIMARY KEY, date DATETIME)", NULL, NULL, &err);
    if (err != NULL)
    {
        LOG("Errrrr: %s", err);
        sqlite3_free(err);
    }

    res = sqlite3_prepare_v2(s_db, "SELECT rank FROM players WHERE username = lower(?)", -1, &s_rank_get_stmt, NULL);
    if (res != SQLITE_OK)
    {
        LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
        sqlite3_close(s_db);
        return;
    }

    res = sqlite3_prepare_v2(s_db, "UPDATE players SET rank = ? WHERE username = lower(?)", -1, &s_rank_set_stmt, NULL);
    if (res != SQLITE_OK)
    {
        LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
        sqlite3_close(s_db);
        return;
    }

    res = sqlite3_prepare_v2(s_db, "INSERT INTO players (username, rank) VALUES (lower(?), 1)", -1, &s_new_user_stmt, NULL);
    if (res != SQLITE_OK)
    {
        LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
        sqlite3_close(s_db);
        return;
    }

    res = sqlite3_prepare_v2(s_db, "SELECT id FROM players WHERE username = lower(?)", -1, &s_globalid_get_stmt, NULL);
    if (res != SQLITE_OK)
    {
        LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
        sqlite3_close(s_db);
        return;
    }

    res = sqlite3_prepare_v2(s_db, "SELECT username FROM players WHERE id = ?", -1, &s_username_get_stmt, NULL);
    if (res != SQLITE_OK)
    {
        LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
        sqlite3_close(s_db);
        return;
    }

    res = sqlite3_prepare_v2(s_db, "SELECT COUNT(*) FROM players WHERE username = lower(?) AND password = ?", -1, &s_password_stmt, NULL);
    if (res != SQLITE_OK)
    {
        LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
        sqlite3_close(s_db);
        return;
    }
}

void playerdb_close()
{
    sqlite3_finalize(s_rank_get_stmt);
    sqlite3_finalize(s_rank_set_stmt);
    sqlite3_finalize(s_new_user_stmt);
    sqlite3_finalize(s_globalid_get_stmt);
    sqlite3_finalize(s_username_get_stmt);
    sqlite3_finalize(s_password_stmt);
    sqlite3_close(s_db);
}

int playerdb_get_globalid(const char *username, bool add)
{
    int i;
    int res;

    for (i = 0; i < (add ? 2 : 1); i++)
    {
        sqlite3_reset(s_globalid_get_stmt);
        if (sqlite3_bind_text(s_globalid_get_stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK)
        {
			LOG("[playerid_get_globalid] Can't bind text: %s\n", sqlite3_errmsg(s_db));
			return -1;
		}
        res = sqlite3_step(s_globalid_get_stmt);
        if (res == SQLITE_ROW)
        {
            return sqlite3_column_int(s_globalid_get_stmt, 0);
        }

		if (i == 0 && add)
		{
			/* New user! */
			sqlite3_reset(s_new_user_stmt);
			if (sqlite3_bind_text(s_new_user_stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK)
			{
				LOG("[playerdb_get_globalid] Can't bind text: %s\n", sqlite3_errmsg(s_db));
				return -1;
			}
			res = sqlite3_step(s_new_user_stmt);
			if (res != SQLITE_DONE)
			{
				LOG("Unable to create new user '%s'\n", username);
				return -1;
			}
		}
    }

    LOG("Unable to get globalid for '%s'", username);
    return -1;
}

const char *playerdb_get_username(int globalid)
{
    int res;
    sqlite3_reset(s_username_get_stmt);
    if (sqlite3_bind_int(s_username_get_stmt, 1, globalid) != SQLITE_OK)
	{
		LOG("[playerdb_get_username] Can't bind int: %s\n", sqlite3_errmsg(s_db));
		return "unknown";
	}
    res = sqlite3_step(s_username_get_stmt);
    if (res == SQLITE_ROW)
    {
        return (const char *)sqlite3_column_text(s_username_get_stmt, 0);
    }

    return "unknown";
}

int playerdb_get_rank(const char *username)
{
    int res;
    sqlite3_reset(s_rank_get_stmt);
    if (sqlite3_bind_text(s_rank_get_stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK)
    {
		LOG("[playerdb_get_rank] Can't bind text: %s\n", sqlite3_errmsg(s_db));
		return RANK_GUEST;
    }
    res = sqlite3_step(s_rank_get_stmt);
    if (res == SQLITE_ROW)
    {
        return sqlite3_column_int(s_rank_get_stmt, 0);
    }

    LOG("Unable to get rank for '%s'", username);

    /* New user! */
    /*sqlite3_reset(s_new_user_stmt);
    sqlite3_bind_text(s_new_user_stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_step(s_new_user_stmt);*/

    /* Default rank */
    return RANK_GUEST;
}

void playerdb_set_rank(const char *username, int rank)
{
    sqlite3_reset(s_rank_set_stmt);
    if (sqlite3_bind_int(s_rank_set_stmt, 1, rank) != SQLITE_OK)
    {
		LOG("[playerdb_set_rank] Can't bind int: %s\n", sqlite3_errmsg(s_db));
		return;
    }
    if (sqlite3_bind_text(s_rank_set_stmt, 2, username, -1, SQLITE_STATIC) != SQLITE_OK)
    {
		LOG("[playerdb_set_rank] Can't bind text: %s\n", sqlite3_errmsg(s_db));
		return;
    }
    if (sqlite3_step(s_rank_set_stmt) != SQLITE_DONE)
    {
    	LOG("[playerdb_set_rank] %s\n", sqlite3_errmsg(s_db));
    	return;
    }
}

int playerdb_password_check(const char *username, const char *password)
{
    sqlite3_reset(s_password_stmt);
    if (sqlite3_bind_text(s_password_stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK)
    {
		LOG("[playerdb_password_check] Can't bind text: %s\n", sqlite3_errmsg(s_db));
		return 0;
    }
    if (sqlite3_bind_text(s_password_stmt, 1, password, -1, SQLITE_STATIC) != SQLITE_OK)
    {
		LOG("[playerdb_password_check] Can't bind text: %s\n", sqlite3_errmsg(s_db));
		return 0;
    }
    if (sqlite3_step(s_password_stmt) != SQLITE_ROW)
    {
    	LOG("[playerdb_password_check] %s\n", sqlite3_errmsg(s_db));
    	return 0;
    }
    return sqlite3_column_int(s_password_stmt, 0);
}

