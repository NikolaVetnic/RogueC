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

/* Up stairs anywhere; down stairs at least half the longest walk away. */
static bool place_stairs(Level *lv, Rng *rng, int depth, int bottom)
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
        if (depth < bottom)
        {
            ok = path_distances(m, lv->stairs_up, dist);
            int far = 0;
            for (size_t i = 0; ok && i < n; i++)
                if (dist[i] > far)
                    far = dist[i];
            if (ok)
                lv->stairs_down = pick_cell(m, rng, dist, far / 2 > 1 ? far / 2 : 1, cells);
        }
    }
    if (ok && depth > 1)
        map_set(m, lv->stairs_up.x, lv->stairs_up.y, TILE_STAIRS_UP);
    if (ok && lv->stairs_down.x >= 0)
        map_set(m, lv->stairs_down.x, lv->stairs_down.y, TILE_STAIRS_DOWN);

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
        ok = place_stairs(lv, &rng, depth, bottom);
    if (!ok)
        level_free(lv);
    return ok;
}

void level_free(Level *lv)
{
    map_free(&lv->map);
}
