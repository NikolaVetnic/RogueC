/* Room-and-corridor levels: rectangular rooms joined by L-shaped corridors. */
#ifndef ROGUEC_GEN_ROOMS_H
#define ROGUEC_GEN_ROOMS_H

#include "core/map.h"
#include "core/rng.h"

enum
{
    ROOMS_MAX = 32,
};

typedef struct
{
    int max_rooms;    /* at most ROOMS_MAX */
    int attempts;     /* random placements tried */
    int min_w, max_w; /* interior size */
    int min_h, max_h;
    int extra_link_pct; /* chance per room of a second corridor to a random room (loops) */
} RoomParams;

extern const RoomParams ROOM_DEFAULTS;

/* Fills m with rooms and corridors, one 4-connected floor region bordered by
 * wall. Returns the number of rooms (at least 2), or 0 if none fit. */
int gen_rooms(Map *m, Rng *rng, const RoomParams *p);

#endif
