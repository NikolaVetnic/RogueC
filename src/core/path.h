/* Distance maps: how many steps from one cell to every other. */
#ifndef ROGUEC_PATH_H
#define ROGUEC_PATH_H

#include <stdbool.h>

#include "core/map.h"
#include "core/vec2.h"

/* Fills dist[w * h] (row-major) with the number of moves from `from` to each
 * cell over walkable cells, 8 directions like the player; -1 where it can't be
 * reached. Breadth-first search. Returns false if out of memory. */
bool path_distances(const Map *m, Vec2 from, int *dist);

#endif
