#include "export.h"

#include "gamedata.h"
#include "faction.h"
#include "region.h"

#include <cJSON.h>

#include <assert.h>

static void json_to_cr(cJSON *json, FILE *F)
{
    if (json->type == cJSON_Array) {
        cJSON *child = json->child;
        while (child) {
            if (child->type == cJSON_Object) {
                cJSON *jId = cJSON_GetObjectItem(child, "id");
                if (jId) {
                    fprintf(F, "%s %d\n", json->string, jId->valueint);
                }
                json_to_cr(child, F);
            }
            else if (child->type == cJSON_String) {
                fprintf(F, "\"%s\";%s\n", child->valuestring, child->string);
            }
            child = child->next;
        }
    }
    else if (json->type == cJSON_Object) {
        cJSON *child = json->child;
        while (child) {
            if (child->type == cJSON_Object) {
                fprintf(F, "%s\n", child->string);
            }
            json_to_cr(child, F);
            child = child->next;
        }
    }
    else if (json->type == cJSON_Number) {
        fprintf(F, "%d;%s\n", json->valueint, json->string);
    }
    else if (json->type == cJSON_String) {
        fprintf(F, "\"%s\";%s\n", json->valuestring, json->string);
    }
}

static int cr_faction(faction *f, void *arg)
{
    FILE * F = (FILE *)arg;
    fprintf(F, "PARTEI %u\n", f->id);
    if (f->data) {
        json_to_cr(f->data, F);
    }
    return 0;
}

static int cr_region(region *r, void *arg)
{
    FILE * F = (FILE *)arg;
    if (r->loc.z == 0) {
        fprintf(F, "REGION %d %d\n", r->loc.x, r->loc.y);
    }
    else {
        fprintf(F, "REGION %d %d %d\n", r->loc.x, r->loc.y, r->loc.z);
    }
    if (r->data) {
        json_to_cr(r->data, F);
    }
    return 0;
}

int export(struct gamedata *gd, FILE *F)
{
    int err;
    assert(gd);

    fprintf(F, "VERSION 66\n%d;Runde\n36;Basis\n", game_get_turn(gd));
    err = factions_walk(gd, cr_faction, F);
    if (err != 0) return err;
    err = regions_walk(gd, cr_region, F);
    if (err != 0) return err;
    return 0;
}