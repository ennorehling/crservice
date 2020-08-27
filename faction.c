#include "faction.h"
#include "message.h"

#include "stb_ds.h"
#include "stretchy_buffer.h"

#include <strings.h>
#include <cJSON.h>

#include <string.h>

void faction_free(faction *f)
{
    int i, count = stb_sb_count(f->messages);
    for (i = 0; i != count; ++i) {
        message_free(f->messages + i);
    }
    stb_sb_free(f->messages);
    cJSON_Delete(f->data);
    free(f->name);
    free(f->email);
}

faction *create_faction(void)
{
    faction * f = calloc(1, sizeof(faction));
    return f;
}

void factions_free(factions *all)
{
    int i, len = stbds_arrlen(all->arr);
    for (i = 0; i != len; ++i) {
        faction_free(all->arr[i]);
        free(all->arr[i]);
    }
    stbds_arrfree(all->arr);
    stbds_hmfree(all->hash_uid);
}

void factions_add(factions *all, faction *f)
{
    unsigned int index;
    stbds_arrput(all->arr, f);
    index = stbds_arrlen(all->arr);
    stbds_hmput(all->hash_uid, f->id, index);
}

faction *factions_get(factions *all, faction_id uid)
{
    struct faction_index_uid *index;
    index = stbds_hmgetp(all->hash_uid, uid);
    if (index) {
        return all->arr[index->value];
    }
    return NULL;
}
