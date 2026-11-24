/* Field of view by ray casting. */
#ifndef ROGUEC_FOV_H
#define ROGUEC_FOV_H

#include "core/map.h"
#include "core/vec2.h"

/* Recomputes m->visible as seen from `origin` within `radius` cells and marks
 * those cells explored. Casts one Bresenham ray to every cell on the square of
 * that radius, so every cell inside is reached; a ray lights the first opaque
 * cell it hits (walls are seen) and stops there. */
void fov_compute(Map *m, Vec2 origin, int radius);

#endif
