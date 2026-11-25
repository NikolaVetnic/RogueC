#include "core/level.h"

#include <stdlib.h>

#include "core/gen_cave.h"
#include "core/gen_rooms.h"
#include "core/path.h"
#include "core/rng.h"

int level_cave_chance(int depth)
{
    int pct = (depth - 3) * 25;
    return pct < 0 ? 0 : pct > 100 ? 100 : pct;
}

/* Floor with floor all around: in a room or open cave, not in a corridor. */
static bool open_floor(const Map *m, int x, int y)
{
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (map_get(m, x + dx, y + dy) != TILE_FLOOR)
                return false;
    return true;
}

/* A random floor cell with dist >= min_dist (dist may be NULL), preferring
 * open floor. `cells` is scratch space for w * h ints. (-1, -1) if none. */
static Vec2 pick_cell(const Map *m, Rng *rng, const int *dist, int min_dist, int *cells)
{
    for (int want_open = 1; want_open >= 0; want_open--)
    {
        int count = 0;
        for (int y = 0; y < m->h; y++)
            for (int x = 0; x < m->w; x++)
            {
                int i = y * m->w + x;
                if (map_get(m, x, y) == TILE_FLOOR && (!dist || dist[i] >= min_dist) &&
                    (!want_open || open_floor(m, x, y)))
                    cells[count++] = i;
            }
        if (count > 0)
        {
            int i = cells[rng_range(rng, 0, count - 1)];
            return vec2(i % m->w, i / m->w);
        }
    }
    return vec2(-1, -1);
}

/* 3 + depth monsters of the depth's kinds on free floor, at least
 * SPAWN_MIN_DIST steps from the arrival point (dist: distances from it). */
static void spawn_monsters(Level *lv, Rng *rng, int depth, const int *dist, int *cells)
{
    const Map *m = &lv->map;
    int want = 3 + depth < LEVEL_MAX_MONSTERS ? 3 + depth : LEVEL_MAX_MONSTERS;
    for (int i = 0; i < want; i++)
    {
        int count = 0;
        for (int y = 0; y < m->h; y++)
            for (int x = 0; x < m->w; x++)
                if (map_get(m, x, y) == TILE_FLOOR && dist[y * m->w + x] >= SPAWN_MIN_DIST &&
                    level_monster_at(lv, vec2(x, y)) < 0)
                    cells[count++] = y * m->w + x;
        if (count == 0)
            return;
        int c = cells[rng_range(rng, 0, count - 1)];
        level_add_monster(
            lv, monster_make(monster_pick_kind(rng, depth), vec2(c % m->w, c / m->w), rng));
    }
}

/* Up stairs on open floor; down stairs at least half the longest walk away;
 * then the monsters. */
static bool furnish(Level *lv, Rng *rng, int depth, int bottom)
{
    Map *m = &lv->map;
    size_t n = (size_t)m->w * (size_t)m->h;
    int *dist = malloc(n * sizeof *dist);
    int *cells = malloc(n * sizeof *cells);
    bool ok = dist && cells;

    if (ok)
    {
        lv->stairs_up = pick_cell(m, rng, NULL, 0, cells);
        lv->stairs_down = vec2(-1, -1);
        ok = path_distances(m, lv->stairs_up, dist);
    }
    if (ok && depth < bottom)
    {
        int far = 0;
        for (size_t i = 0; i < n; i++)
            if (dist[i] > far)
                far = dist[i];
        lv->stairs_down = pick_cell(m, rng, dist, far / 2 > 1 ? far / 2 : 1, cells);
    }
    if (ok && depth > 1)
        map_set(m, lv->stairs_up.x, lv->stairs_up.y, TILE_STAIRS_UP);
    if (ok && lv->stairs_down.x >= 0)
        map_set(m, lv->stairs_down.x, lv->stairs_down.y, TILE_STAIRS_DOWN);
    if (ok)
        spawn_monsters(lv, rng, depth, dist, cells);

    free(dist);
    free(cells);
    return ok;
}

bool level_generate(Level *lv, uint64_t seed, int depth, int bottom, int w, int h)
{
    *lv = (Level){0};
    if (!map_init(&lv->map, w, h))
        return false;

    Rng rng;
    rng_seed_stream(&rng, seed, (uint64_t)depth);
    lv->caves = rng_chance(&rng, level_cave_chance(depth));

    bool ok = lv->caves ? gen_cave(&lv->map, &rng, &CAVE_DEFAULTS) > 0
                        : gen_rooms(&lv->map, &rng, &ROOM_DEFAULTS) > 0;
    if (ok)
        ok = furnish(lv, &rng, depth, bottom);
    if (!ok)
        level_free(lv);
    return ok;
}

void level_free(Level *lv)
{
    map_free(&lv->map);
}

int level_monster_at(const Level *lv, Vec2 pos)
{
    for (int i = 0; i < lv->monster_count; i++)
        if (vec2_eq(lv->monsters[i].pos, pos))
            return i;
    return -1;
}

bool level_add_monster(Level *lv, Monster m)
{
    if (lv->monster_count >= LEVEL_MAX_MONSTERS)
        return false;
    lv->monsters[lv->monster_count++] = m;
    return true;
}

void level_remove_monster(Level *lv, int i)
{
    lv->monsters[i] = lv->monsters[--lv->monster_count];
}
