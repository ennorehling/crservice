#pragma once

struct cJSON;

typedef unsigned int faction_id;

typedef struct faction {
    faction_id id;
    char * name;
    char * email;
    struct message *messages; /* stretchy_buffer */
    struct cJSON *data;
} faction;

void faction_free(faction *f);
faction *create_faction(void);

struct faction_index_uid {
    faction_id key;
    unsigned int value;
};

typedef struct factions {
    faction **arr;
    struct faction_index_uid *hash_uid;
} factions;

void factions_free(factions *all);
void factions_add(factions *all, faction *f);
faction *factions_get(factions *all, faction_id uid);
