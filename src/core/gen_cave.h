/* Cave levels from a cellular automaton, trimmed to one connected region. */
#ifndef ROGUEC_GEN_CAVE_H
#define ROGUEC_GEN_CAVE_H

#include "core/map.h"
#include "core/rng.h"

typedef struct
{
    int wall_chance;   /* % of interior cells that start as wall */
    int death_limit;   /* a wall with fewer wall neighbours turns to floor */
    int birth_limit;   /* a floor with at least this many wall neighbours turns to wall */
    int steps;         /* automaton steps */
    int min_floor_pct; /* regenerate while the kept region is a smaller share of the map */
    int max_attempts;
} CaveParams;

extern const CaveParams CAVE_DEFAULTS;

/* Fills m with a cave whose floor is one 4-connected region, bordered by wall.
 * Returns the number of floor cells, or -1 if out of memory. */
int gen_cave(Map *m, Rng *rng, const CaveParams *p);

#endif
