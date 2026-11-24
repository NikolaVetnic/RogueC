/* The level grid: terrain plus what the player sees and has seen. */
#ifndef ROGUEC_MAP_H
#define ROGUEC_MAP_H

#include <stdbool.h>
#include <stdint.h>

/* TILE_WALL is 0 so a zeroed map is solid rock. */
typedef enum
{
    TILE_WALL = 0,
    TILE_FLOOR,
    TILE_COUNT
} Tile;

/* All arrays are row-major, w * h entries: cell (x, y) is index y * w + x. */
typedef struct
{
    int w;
    int h;
    uint8_t *tiles;
    bool *visible;  /* in view this turn */
    bool *explored; /* seen at least once */
} Map;

/* Allocates a w x h map of walls, nothing seen. Returns false if out of memory. */
bool map_init(Map *m, int w, int h);
void map_free(Map *m);

bool map_in_bounds(const Map *m, int x, int y);
/* Cells outside the map read as wall; writes outside are ignored. */
Tile map_get(const Map *m, int x, int y);
void map_set(Map *m, int x, int y, Tile t);
void map_fill(Map *m, Tile t);

bool map_walkable(const Map *m, int x, int y);
bool map_transparent(const Map *m, int x, int y);

bool map_is_visible(const Map *m, int x, int y);
bool map_is_explored(const Map *m, int x, int y);
/* Marks a cell visible this turn and explored for good. */
void map_mark_seen(Map *m, int x, int y);
void map_clear_visible(Map *m);
void map_clear_explored(Map *m);

#endif
