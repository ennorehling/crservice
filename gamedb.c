#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "gamedb.h"
#include "jsondata.h"
#include "faction.h"
#include "region.h"

#include <strings.h>
#include <cJSON.h>
#include <sqlite3.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static sqlite3_stmt *g_stmt_insert_faction;
static sqlite3_stmt *g_stmt_select_faction;
static sqlite3_stmt *g_stmt_insert_region;
static sqlite3_stmt *g_stmt_select_region;

static int db_bind_json(sqlite3_stmt *stmt, int index, cJSON *object) {
    int err;
    char *data = cJSON_PrintUnformatted(object);
    if (data) {
        int sz = (int)strlen(data);
        err = sqlite3_bind_blob(stmt, index, data, sz, SQLITE_TRANSIENT);
        cJSON_free(data);
    }
    else {
        err = sqlite3_bind_null(stmt, index);
    }
    return err;
}

int db_write_faction(sqlite3 *db, const faction *f) {
    int err;
    err = sqlite3_reset(g_stmt_insert_faction);
    if (err != SQLITE_OK) goto db_write_faction_fail;
    err = sqlite3_bind_int64(g_stmt_insert_faction, 1, (sqlite3_int64)f->id);
    if (err != SQLITE_OK) goto db_write_faction_fail;
    err = db_bind_json(g_stmt_insert_faction, 2, f->data);
    if (err != SQLITE_OK) goto db_write_faction_fail;
    err = sqlite3_bind_text(g_stmt_insert_faction, 3, f->name, -1, SQLITE_STATIC);
    if (err != SQLITE_OK) goto db_write_faction_fail;
    err = sqlite3_bind_text(g_stmt_insert_faction, 4, f->email, -1, SQLITE_STATIC);
    if (err != SQLITE_OK) goto db_write_faction_fail;

    err = sqlite3_step(g_stmt_insert_faction);
    if (err != SQLITE_DONE) goto db_write_faction_fail;
    return SQLITE_OK;

db_write_faction_fail:
    err = sqlite3_extended_errcode(db);
    fputs(sqlite3_errmsg(db), stderr);
    return err;
}

faction *db_read_faction(sqlite3 *db, unsigned int id) {
    int err;
    const void *data;

    err = sqlite3_reset(g_stmt_select_faction);
    if (err != SQLITE_OK) goto db_read_faction_fail;
    err = sqlite3_bind_int64(g_stmt_select_faction, 1, (sqlite3_int64)id);
    if (err != SQLITE_OK) goto db_read_faction_fail;
    err = sqlite3_step(g_stmt_select_faction);
    if (err == SQLITE_DONE) {
        return NULL;
    }
    else if (err != SQLITE_ROW) goto db_read_faction_fail;
    data = sqlite3_column_blob(g_stmt_select_faction, 0);
    if (data) {
        cJSON *json = cJSON_Parse(data);
        if (json) {
            faction *f = create_faction(json);
            if (f) {
                f->id = id;
                f->name = str_strdup(sqlite3_column_text(g_stmt_select_faction, 1));
                f->email = str_strdup(sqlite3_column_text(g_stmt_select_faction, 2));
            }
            return f;
        }
    }
    return NULL;

db_read_faction_fail:
    fputs(sqlite3_errmsg(db), stderr);
    return NULL;
}

/*
static sqlite3_stmt *g_stmt_insert_unit;
static sqlite3_stmt *g_stmt_insert_ship;
static sqlite3_stmt *g_stmt_insert_building;
*/

static int db_prepare(sqlite3 *db) {
    int err;
    err = sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO `factions` (`id`, `data`, `name`, `email`) VALUES (?,?,?,?)", -1,
        &g_stmt_insert_faction, NULL);
    err = sqlite3_prepare_v2(db,
        "SELECT `data`, `name`, `email` FROM `factions` WHERE `id` = ?", -1,
        &g_stmt_select_faction, NULL);

    err = sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO `regions` (`id`, `data`, `x`, `y`, `z`, `name`, `terrain`) VALUES (?,?,?,?,?,?,?)", -1,
        &g_stmt_insert_region, NULL);

    err = sqlite3_prepare_v2(db,
        "SELECT `data`, `id`, `name`, `terrain` FROM `regions` WHERE `x` = ? AND `y` = ? AND `z` = ?", -1,
        &g_stmt_select_region, NULL);

/*
    err = sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO `units` (`id`, `data`, `name`, `orders`, `region_id`, `faction_id`) VALUES (?,?,?,?,?,?)", -1,
        &g_stmt_insert_unit, NULL);
    err = sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO `ships` (`id`, `data`, `name`, `region_id`) VALUES (?,?,?,?)", -1,
        &g_stmt_insert_ship, NULL);
    err = sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO `buildings` (`id`, `data`, `name`, `region_id`) VALUES (?,?,?,?)", -1,
        &g_stmt_insert_building, NULL);
*/
    return err;
}

static int db_install(sqlite3 *db, const char *schema) {
    FILE *F = NULL;
    int err;

    F = fopen(schema, "rb");
    if (F) {
        size_t size;
        char *sql;
        fseek(F, 0, SEEK_END);
        size = ftell(F);
        sql = malloc(size + 1);
        if (sql) {
            rewind(F);
            fread(sql, sizeof(char), size, F);
            sql[size] = 0;
            err = sqlite3_exec(db, sql, NULL, NULL, NULL);
            if (err != SQLITE_OK) goto db_install_fail;
        }
        fclose(F);
    }
    return SQLITE_OK;
db_install_fail:
    if (F) fclose(F);
    if (db) {
        fputs(sqlite3_errmsg(db), stderr);
        sqlite3_close(db);
    }
    return err;
}

static int db_upgrade(sqlite3 *db, int from_version, int to_version) {
    int err;
    if (from_version == 0) {
        err = db_install(db, "crschema.sql");
    }
    else {
        int i;
        for (i = from_version + 1; i <= to_version; ++i) {
            char filename[20];
            snprintf(filename, sizeof(filename), "update%2d.sql", i);
            err = db_install(db, filename);
        }
    }
    return err;
}

static int cb_int_col(void *data, int ncols, char **text, char **name) {
    int *p_int = (int *)data;
    (void)ncols;
    (void)name;
    *p_int = atoi(text[0]);
    return SQLITE_OK;
}

int db_open(const char * filename, sqlite3 **dbp, int version) {
    sqlite3 *db = NULL;
    int err, user_version;
    assert(dbp);
    err = sqlite3_open_v2(filename, dbp, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    db = *dbp;
    if (err != SQLITE_OK) goto db_open_fail;
    err = sqlite3_exec(db, "PRAGMA journal_mode=OFF", NULL, NULL, NULL);
    if (err != SQLITE_OK) goto db_open_fail;
    err = sqlite3_exec(db, "PRAGMA synchronous=OFF", NULL, NULL, NULL);
    if (err != SQLITE_OK) goto db_open_fail;
    err = sqlite3_exec(db, "PRAGMA user_version", cb_int_col, &user_version, NULL);
    if (err != SQLITE_OK) goto db_open_fail;
    if (user_version != version) {
        err = db_upgrade(db, user_version, version);
    }
    if (err != SQLITE_OK) goto db_open_fail;
    err = db_prepare(db);
    return SQLITE_OK;
db_open_fail:
    if (db) {
        fputs(sqlite3_errmsg(db), stderr);
        sqlite3_close(db);
    }
    return err;
}

int db_close(sqlite3 * db) {
    int err;
    err = sqlite3_finalize(g_stmt_select_faction);
    g_stmt_select_faction = NULL;
    err = sqlite3_finalize(g_stmt_insert_faction);
    g_stmt_insert_faction = NULL;
    err = sqlite3_finalize(g_stmt_insert_region);
    g_stmt_insert_region = NULL;
    err = sqlite3_finalize(g_stmt_select_region);
    g_stmt_select_region = NULL;
/*
    err = sqlite3_finalize(g_stmt_insert_unit);
    g_stmt_insert_unit = NULL;
    err = sqlite3_finalize(g_stmt_insert_ship);
    g_stmt_insert_ship = NULL;
    err = sqlite3_finalize(g_stmt_insert_building);
    g_stmt_insert_building = NULL;
*/
    err = sqlite3_close(db);
    return err;
}

/*
int db_write_unit(struct sqlite3 *db, const struct unit *u) {
    // INSERT INTO `unit` (`id`, `data`, `name`, `orders`, `region_id`, `faction_id`) VALUES (?,?,?,?,?,?)
    int err;
    assert(u);
    err = sqlite3_reset(g_stmt_insert_unit);
    if (err != SQLITE_OK) goto db_write_unit_fail;
    err = sqlite3_bind_int(g_stmt_insert_unit, 1, u->id);
    if (err != SQLITE_OK) goto db_write_unit_fail;
    err = db_bind_json(g_stmt_insert_unit, 2, u->data);
    if (err != SQLITE_OK) goto db_write_unit_fail;
    err = sqlite3_bind_text(g_stmt_insert_unit, 3, u->name, -1, SQLITE_STATIC);
    if (err != SQLITE_OK) goto db_write_unit_fail;
    if (u->orders) {
        err = sqlite3_bind_text(g_stmt_insert_unit, 4, u->orders, -1, SQLITE_STATIC);
        if (err != SQLITE_OK) goto db_write_unit_fail;
    }
    else {
        err = sqlite3_bind_null(g_stmt_insert_unit, 4);
        if (err != SQLITE_OK) goto db_write_unit_fail;
    }
    if (u->region) {
        err = sqlite3_bind_int(g_stmt_insert_unit, 5, u->region->id);
        if (err != SQLITE_OK) goto db_write_unit_fail;
    }
    else {
        err = sqlite3_bind_null(g_stmt_insert_unit, 5);
        if (err != SQLITE_OK) goto db_write_unit_fail;
    }
    if (u->faction) {
        err = sqlite3_bind_int(g_stmt_insert_unit, 6, u->faction->id);
        if (err != SQLITE_OK) goto db_write_unit_fail;
    }
    else {
        err = sqlite3_bind_null(g_stmt_insert_unit, 6);
        if (err != SQLITE_OK) goto db_write_unit_fail;
    }

    err = sqlite3_step(g_stmt_insert_unit);
    if (err == SQLITE_DONE) {
        return SQLITE_OK;
    }
db_write_unit_fail:
    fputs(sqlite3_errmsg(db), stderr);
    return err;
}

int db_write_ship(struct sqlite3 *db, const struct ship *sh) {
    int err;
    assert(sh);
    err = sqlite3_reset(g_stmt_insert_ship);
    if (err != SQLITE_OK) goto db_write_ship_fail;
    err = sqlite3_bind_int(g_stmt_insert_ship, 1, sh->id);
    if (err != SQLITE_OK) goto db_write_ship_fail;
    err = db_bind_json(g_stmt_insert_ship, 2, sh->data);
    if (err != SQLITE_OK) goto db_write_ship_fail;
    err = sqlite3_bind_text(g_stmt_insert_ship, 3, sh->name, -1, SQLITE_STATIC);
    if (err != SQLITE_OK) goto db_write_ship_fail;
    if (sh->region) {
        err = sqlite3_bind_int(g_stmt_insert_ship, 4, sh->region->id);
        if (err != SQLITE_OK) goto db_write_ship_fail;
    }
    else {
        err = sqlite3_bind_null(g_stmt_insert_ship, 4);
        if (err != SQLITE_OK) goto db_write_ship_fail;
    }

    err = sqlite3_step(g_stmt_insert_ship);
    if (err == SQLITE_DONE) {
        return SQLITE_OK;
    }
db_write_ship_fail:
    fputs(sqlite3_errmsg(db), stderr);
    return err;
}

int db_write_building(struct sqlite3 *db, const struct building *b) {
    int err;
    assert(b);
    err = sqlite3_reset(g_stmt_insert_building);
    if (err != SQLITE_OK) goto db_write_building_fail;
    err = sqlite3_bind_int(g_stmt_insert_building, 1, b->id);
    if (err != SQLITE_OK) goto db_write_building_fail;
    err = db_bind_json(g_stmt_insert_building, 2, b->data);
    if (err != SQLITE_OK) goto db_write_building_fail;
    err = sqlite3_bind_text(g_stmt_insert_building, 3, b->name, -1, SQLITE_STATIC);
    if (err != SQLITE_OK) goto db_write_building_fail;
    if (b->region) {
        err = sqlite3_bind_int(g_stmt_insert_building, 4, b->region->id);
        if (err != SQLITE_OK) goto db_write_building_fail;
    }
    else {
        err = sqlite3_bind_null(g_stmt_insert_building, 4);
        if (err != SQLITE_OK) goto db_write_building_fail;
    }

    err = sqlite3_step(g_stmt_insert_building);
    if (err == SQLITE_DONE) {
        return SQLITE_OK;
    }
db_write_building_fail:
    fputs(sqlite3_errmsg(db), stderr);
    return err;
}
*/

int db_write_region(struct sqlite3 *db, const struct region *r) {
    // INSERT INTO `region` (`id`, `data`, `x`, `y`, `p`, `turn`, `name`, `terrain`) VALUES (?,?,?,?,?,?,?,?)
    int err;
    err = sqlite3_reset(g_stmt_insert_region);
    if (err != SQLITE_OK) goto db_write_region_fail;
    err = sqlite3_bind_int(g_stmt_insert_region, 1, r->id);
    if (err != SQLITE_OK) goto db_write_region_fail;
    err = db_bind_json(g_stmt_insert_region, 2, r->data);
    if (err != SQLITE_OK) goto db_write_region_fail;
    err = sqlite3_bind_int(g_stmt_insert_region, 3, r->x);
    if (err != SQLITE_OK) goto db_write_region_fail;
    err = sqlite3_bind_int(g_stmt_insert_region, 4, r->y);
    if (err != SQLITE_OK) goto db_write_region_fail;
    err = sqlite3_bind_int(g_stmt_insert_region, 5, r->plane);
    if (err != SQLITE_OK) goto db_write_region_fail;
    err = sqlite3_bind_int(g_stmt_insert_region, 6, r->turn);
    if (err != SQLITE_OK) goto db_write_region_fail;
    if (r->name) {
        err = sqlite3_bind_text(g_stmt_insert_region, 7, r->name, -1, SQLITE_STATIC);
    }
    else {
        err = sqlite3_bind_null(g_stmt_insert_region, 7);
    }
    if (err != SQLITE_OK) goto db_write_region_fail;
    err = sqlite3_bind_text(g_stmt_insert_region, 8, terrainname[r->terrain], -1, SQLITE_STATIC);
    if (err != SQLITE_OK) goto db_write_region_fail;

    err = sqlite3_step(g_stmt_insert_region);
    if (err != SQLITE_DONE) goto db_write_region_fail;
    return SQLITE_OK;

db_write_region_fail:
    fputs(sqlite3_errmsg(db), stderr);
    return err;
}

region *db_read_region(struct sqlite3 *db, int x, int y, int z) {
    int err;
    const void *data;

    err = sqlite3_reset(g_stmt_select_region);
    if (err != SQLITE_OK) goto db_read_region_fail;
    err = sqlite3_bind_int(g_stmt_select_region, 1, x);
    err = sqlite3_bind_int(g_stmt_select_region, 2, y);
    err = sqlite3_bind_int(g_stmt_select_region, 3, z);
    if (err != SQLITE_OK) goto db_read_region_fail;
    err = sqlite3_step(g_stmt_select_region);
    if (err == SQLITE_DONE) {
        return NULL;
    }
    else if (err != SQLITE_ROW) goto db_read_region_fail;
    data = sqlite3_column_blob(g_stmt_select_region, 0);
    if (data) {
        cJSON *json = cJSON_Parse(data);
        if (json) {
            region *r = create_region(json);
            r->id = sqlite3_column_int(g_stmt_select_region, 1);
            r->name = str_strdup(sqlite3_column_blob(g_stmt_select_region, 2));
            r->terrain = get_terrain(sqlite3_column_blob(g_stmt_select_region, 3));
            return r;
        }
    }
    return NULL;

db_read_region_fail:
    fputs(sqlite3_errmsg(db), stderr);
    return NULL;
}

int db_region_delete_objects(sqlite3 *db, unsigned int region_id)
{
    char zSQL[80];
    int err;
    sprintf(zSQL, "DELETE FROM buildings WHERE region_id=%u", region_id);
    if (SQLITE_OK != (err = sqlite3_exec(db, zSQL, NULL, NULL, NULL))) goto db_delete_objects_fail;
    sprintf(zSQL, "DELETE FROM ships WHERE region_id=%u", region_id);
    if (SQLITE_OK != (err = sqlite3_exec(db, zSQL, NULL, NULL, NULL))) goto db_delete_objects_fail;
    sprintf(zSQL, "DELETE FROM units WHERE region_id=%u", region_id);
    if (SQLITE_OK != (err = sqlite3_exec(db, zSQL, NULL, NULL, NULL))) goto db_delete_objects_fail;
    return err;

db_delete_objects_fail:
    fputs(sqlite3_errmsg(db), stderr);
    return err;
}
