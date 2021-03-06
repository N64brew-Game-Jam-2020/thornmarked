#pragma once

#include "game/core/entity.h"

#include <stdbool.h>

struct game_state;

// A player component.
struct cp_player {
    ent_id ent;
    int state;
    float state_time;
};

// The player system.
struct sys_player {
    struct cp_player players[PLAYER_COUNT];
};

// Initialize player system.
void player_init(struct sys_player *restrict psys);

// Destroy all components.
inline void player_destroyall(struct sys_player *restrict psys) {
    for (int i = 0; i < PLAYER_COUNT; i++) {
        psys->players[i].ent.id = 0;
    }
}

// Get the player compenent for the given entity, or NULL if there is none.
struct cp_player *player_get(struct sys_player *restrict psys, ent_id ent);

// Create new player component attached to the given entity, using the given
// controller index. Overwrites any existing player component for that entity or
// for that input index.
struct cp_player *player_new(struct sys_player *restrict msys, int player_index,
                             ent_id ent);

// Spawn a player entity.
void player_spawn(struct game_state *restrict gs, int player_index);

// Update players.
void player_update(struct game_state *restrict gs, float dt);
