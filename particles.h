/* particles.h - additive glowing point-sprite particles (embers, sparks, trails) */
#ifndef PARTICLES_H
#define PARTICLES_H

#include "math3d.h"

void particles_init(void);   /* build GL program + buffers (needs a GL context) */
void particles_spawn(v3 pos, v3 vel, v3 color, float life, float size);
void particles_burst(v3 pos, v3 color, int n, float speed);
void particles_update(float dt);
void particles_render(mat4 vp); /* uploads live particles and draws them additively */

#endif /* PARTICLES_H */
