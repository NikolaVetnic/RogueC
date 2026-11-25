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

/* A random free floor cell (no stairs, no item); false if none after many tries. */
static bool free_floor(const Level *lv, Rng *rng, Vec2 *out)
{
    const Map *m = &lv->map;
    for (int tries = 0; tries < 1000; tries++)
    {
        Vec2 p = vec2(rng_range(rng, 0, m->w - 1), rng_range(rng, 0, m->h - 1));
        if (map_get(m, p.x, p.y) == TILE_FLOOR && level_item_at(lv, p) < 0)
        {
            *out = p;
            return true;
        }
    }
    return false;
}

static void spawn_items(Level *lv, Rng *rng, int depth)
{
    Vec2 p;
    int piles = rng_range(rng, 2, 4);
    for (int i = 0; i < piles && free_floor(lv, rng, &p); i++)
        level_add_item(lv, item_gold(rng, depth), p);
    int items = 3 + depth / 3;
    for (int i = 0; i < items && free_floor(lv, rng, &p); i++)
        level_add_item(lv, item_random(rng, depth), p);
}

/* Up stairs on open floor; down stairs at least half the longest walk away;
 * then the monsters and items. */
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
    Vec2 far_cell = vec2(-1, -1); /* the down stairs, or on the bottom level the goal */
    if (ok)
    {
        int far = 0;
        for (size_t i = 0; i < n; i++)
            if (dist[i] > far)
                far = dist[i];
        far_cell = pick_cell(m, rng, dist, far / 2 > 1 ? far / 2 : 1, cells);
    }
    if (ok && depth < bottom)
        lv->stairs_down = far_cell;
    if (ok) /* on depth 1, the way out */
        map_set(m, lv->stairs_up.x, lv->stairs_up.y, TILE_STAIRS_UP);
    if (ok && lv->stairs_down.x >= 0)
        map_set(m, lv->stairs_down.x, lv->stairs_down.y, TILE_STAIRS_DOWN);
    if (ok)
    {
        if (depth == bottom)
            level_add_item(lv, (Item){.cls = ITEM_GOAL, .count = 1}, far_cell);
        spawn_monsters(lv, rng, depth, dist, cells);
        spawn_items(lv, rng, depth);
    }

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

int level_item_at(const Level *lv, Vec2 pos)
{
    for (int i = 0; i < lv->item_count; i++)
        if (vec2_eq(lv->items[i].pos, pos))
            return i;
    return -1;
}

bool level_add_item(Level *lv, Item it, Vec2 pos)
{
    if (lv->item_count >= LEVEL_MAX_ITEMS || level_item_at(lv, pos) >= 0)
        return false;
    it.equipped = false;
    lv->items[lv->item_count++] = (FloorItem){.item = it, .pos = pos};
    return true;
}

void level_remove_item(Level *lv, int i)
{
    lv->items[i] = lv->items[--lv->item_count];
}
