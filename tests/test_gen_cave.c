#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "core/gen_cave.h"

/* Number of floor cells reachable from the first floor cell by 4-way steps. */
static int reachable_floor(const Map *m)
{
    int n = m->w * m->h, start = -1;
    for (int i = 0; i < n && start < 0; i++)
        if (m->tiles[i] == TILE_FLOOR)
            start = i;
    if (start < 0)
        return 0;

    bool *seen = calloc((size_t)n, sizeof *seen);
    int *queue = malloc((size_t)n * sizeof *queue);
    int head = 0, tail = 0;
    seen[start] = true;
    queue[tail++] = start;

    while (head < tail)
    {
        int i = queue[head++], x = i % m->w, y = i / m->w;
        const int nb[4][2] = {{x + 1, y}, {x - 1, y}, {x, y + 1}, {x, y - 1}};
        for (int d = 0; d < 4; d++)
        {
            int nx = nb[d][0], ny = nb[d][1], j = ny * m->w + nx;
            if (map_get(m, nx, ny) == TILE_FLOOR && !seen[j])
            {
                seen[j] = true;
                queue[tail++] = j;
            }
        }
    }
    free(seen);
    free(queue);
    return tail;
}

static int count_floor(const Map *m)
{
    int c = 0;
    for (int i = 0; i < m->w * m->h; i++)
        c += m->tiles[i] == TILE_FLOOR;
    return c;
}

static void caves_are_connected_bordered_and_big_enough(void)
{
    Map m;
    map_init(&m, 100, 42);
    int disconnected = 0, open_border = 0, too_small = 0;

    for (uint64_t seed = 0; seed < 200; seed++)
    {
        Rng r;
        rng_seed(&r, seed);
        int floor = gen_cave(&m, &r, &CAVE_DEFAULTS);

        disconnected += floor != count_floor(&m) || reachable_floor(&m) != floor;
        too_small += floor * 100 < m.w * m.h * CAVE_DEFAULTS.min_floor_pct;
        for (int x = 0; x < m.w; x++)
            open_border += map_get(&m, x, 0) != TILE_WALL || map_get(&m, x, m.h - 1) != TILE_WALL;
        for (int y = 0; y < m.h; y++)
            open_border += map_get(&m, 0, y) != TILE_WALL || map_get(&m, m.w - 1, y) != TILE_WALL;
    }
    CHECK(disconnected == 0);
    CHECK(open_border == 0);
    CHECK(too_small == 0);
    map_free(&m);
}

static void same_seed_same_cave(void)
{
    Map a, b;
    map_init(&a, 60, 30);
    map_init(&b, 60, 30);
    Rng ra, rb;
    rng_seed(&ra, 1234);
    rng_seed(&rb, 1234);
    gen_cave(&a, &ra, &CAVE_DEFAULTS);
    gen_cave(&b, &rb, &CAVE_DEFAULTS);
    CHECK(memcmp(a.tiles, b.tiles, 60 * 30) == 0);

    rng_seed(&rb, 1235);
    gen_cave(&b, &rb, &CAVE_DEFAULTS);
    CHECK(memcmp(a.tiles, b.tiles, 60 * 30) != 0);
    map_free(&a);
    map_free(&b);
}

void run_gen_cave_tests(void)
{
    RUN(caves_are_connected_bordered_and_big_enough);
    RUN(same_seed_same_cave);
}
