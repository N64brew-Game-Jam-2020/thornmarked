#pragma once

enum {
    // Maximum number of entities.
    ENTITY_COUNT = 16,
};

// An entity ID. The ID must be in the range 0..ENTITY_COUNT-1.
typedef struct ent_id {
    int id;
} ent_id;
