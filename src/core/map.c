#include "core/map.h"

#include <stdlib.h>
#include <string.h>

static const struct
{
    bool walkable;
    bool transparent;
} TILE_INFO[TILE_COUNT] = {
    [TILE_WALL] = {false, false},
    [TILE_FLOOR] = {true, true},
};

static size_t cell_count(const Map *m)
{
    return (size_t)m->w * (size_t)m->h;
}

static size_t index_of(const Map *m, int x, int y)
{
    return (size_t)y * (size_t)m->w + (size_t)x;
}

bool map_init(Map *m, int w, int h)
{
    *m = (Map){.w = w, .h = h};
    size_t n = cell_count(m);

    m->tiles = calloc(n, sizeof *m->tiles);
    m->visible = calloc(n, sizeof *m->visible);
    m->explored = calloc(n, sizeof *m->explored);

    if (!m->tiles || !m->visible || !m->explored)
    {
        map_free(m);
        return false;
    }
    return true;
}

void map_free(Map *m)
{
    free(m->tiles);
    free(m->visible);
    free(m->explored);
    *m = (Map){0};
}

bool map_in_bounds(const Map *m, int x, int y)
{
    return x >= 0 && y >= 0 && x < m->w && y < m->h;
}

Tile map_get(const Map *m, int x, int y)
{
    return map_in_bounds(m, x, y) ? (Tile)m->tiles[index_of(m, x, y)] : TILE_WALL;
}

void map_set(Map *m, int x, int y, Tile t)
{
    if (map_in_bounds(m, x, y))
        m->tiles[index_of(m, x, y)] = (uint8_t)t;
}

void map_fill(Map *m, Tile t)
{
    memset(m->tiles, (int)t, cell_count(m));
}

bool map_walkable(const Map *m, int x, int y)
{
    return TILE_INFO[map_get(m, x, y)].walkable;
}

bool map_transparent(const Map *m, int x, int y)
{
    return TILE_INFO[map_get(m, x, y)].transparent;
}

bool map_is_visible(const Map *m, int x, int y)
{
    return map_in_bounds(m, x, y) && m->visible[index_of(m, x, y)];
}

bool map_is_explored(const Map *m, int x, int y)
{
    return map_in_bounds(m, x, y) && m->explored[index_of(m, x, y)];
}

void map_mark_seen(Map *m, int x, int y)
{
    if (!map_in_bounds(m, x, y))
        return;
    size_t i = index_of(m, x, y);
    m->visible[i] = true;
    m->explored[i] = true;
}

void map_clear_visible(Map *m)
{
    memset(m->visible, 0, cell_count(m) * sizeof *m->visible);
}

void map_clear_explored(Map *m)
{
    memset(m->explored, 0, cell_count(m) * sizeof *m->explored);
}
