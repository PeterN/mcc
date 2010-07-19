#ifndef PLAYERDB_H
#define PLAYERDB_H

void playerdb_init();
void playerdb_close();

int playerdb_get_globalid(const char *username, bool add, bool *added);
const char *playerdb_get_username(int globalid);
int playerdb_get_rank(const char *username);
void playerdb_set_rank(const char *username, int rank);
int playerdb_password_check(const char *username, const char *password);

#endif /* PLAYERDB_H */
