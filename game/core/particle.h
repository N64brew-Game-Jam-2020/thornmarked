#pragma once

#include "base/vectypes.h"

// A simple particle. This is not a component and cannot be attached to an
// entity.
struct particle {
    vec3 pos;
};

// Particle system.
struct sys_particle {
    struct particle *particle;
    int count;
};

// Initialize the particle system.
void particle_init(struct sys_particle *restrict psys);

// Create a new particle at the given position.
void particle_create(struct sys_particle *restrict psys, vec3 pos);
