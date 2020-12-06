#include "game/core/monster.h"

#include "base/base.h"
#include "game/core/physics.h"
#include "game/core/random.h"
#include "game/core/walk.h"

enum {
    // Maximum number of monster components.
    MONSTER_COUNT = 8,
};

void monster_init(struct sys_monster *restrict msys) {
    *msys = (struct sys_monster){
        .monsters = mem_alloc(sizeof(*msys->monsters) * MONSTER_COUNT),
        .entities = mem_calloc(sizeof(*msys->entities) * ENTITY_COUNT),
    };
}

struct cp_monster *monster_get(struct sys_monster *restrict msys, ent_id ent);

struct cp_monster *monster_new(struct sys_monster *restrict msys, ent_id ent) {
    int index = msys->entities[ent.id];
    struct cp_monster *p = &msys->monsters[index];
    if (p->ent.id != ent.id || index >= msys->count) {
        index = msys->count;
        if (index >= MONSTER_COUNT) {
            fatal_error("Too many monsters");
        }
        p = &msys->monsters[index];
        msys->count = index + 1;
    }
    *p = (struct cp_monster){
        .ent = ent,
    };
    return p;
}

void monster_update(struct sys_monster *restrict msys,
                    struct sys_phys *restrict psys,
                    struct sys_walk *restrict wsys, float dt) {
    (void)psys;
    for (struct cp_monster *mp = msys->monsters, *me = mp + msys->count;
         mp != me; mp++) {
        mp->timer -= dt;
        if (mp->timer <= 0.0f) {
            mp->timer = rand_frange(&grand, 1.0f, 2.0f);
            struct cp_walk *restrict wp = walk_get(wsys, mp->ent);
            if (wp != NULL) {
                wp->drive = (vec2){{rand_frange(&grand, -1.0f, 1.0f),
                                    rand_frange(&grand, -1.0f, 1.0f)}};
            }
        }
    }
}