#include "core/gen_cave.h"

#include <stdlib.h>
#include <string.h>

/* The 2022 parameters: open caves with scattered pillars. */
const CaveParams CAVE_DEFAULTS = {
    .wall_chance = 15,
    .death_limit = 1,
    .birth_limit = 4,
    .steps = 5,
    .min_floor_pct = 30,
    .max_attempts = 20,
};

static bool on_border(const Map *m, int x, int y)
{
    return x == 0 || y == 0 || x == m->w - 1 || y == m->h - 1;
}

static void seed_noise(Map *m, Rng *rng, const CaveParams *p)
{
    for (int y = 0; y < m->h; y++)
        for (int x = 0; x < m->w; x++)
        {
            bool wall = on_border(m, x, y) || rng_chance(rng, p->wall_chance);
            map_set(m, x, y, wall ? TILE_WALL : TILE_FLOOR);
        }
}

/* Wall neighbours of (x, y) among its 8, in the previous generation `prev`.
 * Cells outside the map count as wall. */
static int wall_neighbours(const Map *m, const uint8_t *prev, int x, int y)
{
    int count = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
        {
            if (dx == 0 && dy == 0)
                continue;
            int nx = x + dx, ny = y + dy;
            if (!map_in_bounds(m, nx, ny) || prev[ny * m->w + nx] == TILE_WALL)
                count++;
        }
    return count;
}

/* One automaton step. Reads a copy of the previous generation so that cells
 * updated earlier in this step don't influence the rest. */
static void automaton_step(Map *m, uint8_t *prev, const CaveParams *p)
{
    memcpy(prev, m->tiles, (size_t)m->w * (size_t)m->h);

    for (int y = 0; y < m->h; y++)
        for (int x = 0; x < m->w; x++)
        {
            int n = wall_neighbours(m, prev, x, y);
            bool wall = prev[y * m->w + x] == TILE_WALL ? n >= p->death_limit : n >= p->birth_limit;
            if (on_border(m, x, y))
                wall = true;
            map_set(m, x, y, wall ? TILE_WALL : TILE_FLOOR);
        }
}

/* Labels 4-connected floor regions by breadth-first search and turns every
 * region but the largest into wall. `label` and `queue` hold w * h ints.
 * Returns the size of the kept region. */
static int keep_largest_region(Map *m, int *label, int *queue)
{
    int n = m->w * m->h;
    memset(label, 0, (size_t)n * sizeof *label);

    int best_label = 0, best_size = 0, next_label = 1;
    static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};

    for (int start = 0; start < n; start++)
    {
        if (m->tiles[start] != TILE_FLOOR || label[start] != 0)
            continue;

        int head = 0, tail = 0, size = 0;
        label[start] = next_label;
        queue[tail++] = start;

        while (head < tail)
        {
            int i = queue[head++];
            int x = i % m->w, y = i / m->w;
            size++;

            for (int d = 0; d < 4; d++)
            {
                int nx = x + DX[d], ny = y + DY[d];
                if (map_get(m, nx, ny) != TILE_FLOOR)
                    continue;
                int j = ny * m->w + nx;
                if (label[j] == 0)
                {
                    label[j] = next_label;
                    queue[tail++] = j;
                }
            }
        }

        if (size > best_size)
        {
            best_size = size;
            best_label = next_label;
        }
        next_label++;
    }

    for (int i = 0; i < n; i++)
        if (m->tiles[i] == TILE_FLOOR && label[i] != best_label)
            m->tiles[i] = TILE_WALL;

    return best_size;
}

int gen_cave(Map *m, Rng *rng, const CaveParams *p)
{
    size_t n = (size_t)m->w * (size_t)m->h;
    uint8_t *prev = malloc(n);
    int *label = malloc(n * sizeof *label);
    int *queue = malloc(n * sizeof *queue);
    int floor = -1;

    if (prev && label && queue)
    {
        for (int attempt = 0; attempt < p->max_attempts; attempt++)
        {
            seed_noise(m, rng, p);
            for (int s = 0; s < p->steps; s++)
                automaton_step(m, prev, p);
            floor = keep_largest_region(m, label, queue);
            if ((size_t)floor * 100 >= n * (size_t)p->min_floor_pct)
                break;
        }
    }

    free(prev);
    free(label);
    free(queue);
    return floor;
}
