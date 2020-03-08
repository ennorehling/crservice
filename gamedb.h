#pragma once

struct sqlite3;
struct unit;
struct faction;
struct region;
struct ship;
struct building;

int db_open(const char * filename, struct sqlite3 **dbp, int version);
int db_close(struct sqlite3 *db);

int db_write_unit(struct sqlite3 *db, const struct unit *u);
int db_write_ship(struct sqlite3 *db, const struct ship *sh);
int db_write_building(struct sqlite3 *db, const struct building *b);
int db_write_region(struct sqlite3 *db, const struct region *r);