#include "test.h"

#include "core/map.h"

static void new_map_is_solid_and_unseen(void)
{
    Map m;
    CHECK(map_init(&m, 5, 4));
    CHECK(m.w == 5 && m.h == 4);
    for (int y = 0; y < m.h; y++)
        for (int x = 0; x < m.w; x++)
        {
            CHECK(map_get(&m, x, y) == TILE_WALL);
            CHECK(!map_is_visible(&m, x, y) && !map_is_explored(&m, x, y));
        }
    map_free(&m);
    CHECK(m.tiles == NULL);
}

static void outside_reads_as_wall_and_writes_are_ignored(void)
{
    Map m;
    map_init(&m, 3, 3);
    map_fill(&m, TILE_FLOOR);
    map_set(&m, -1, 0, TILE_FLOOR);
    map_set(&m, 3, 3, TILE_FLOOR);
    CHECK(map_get(&m, -1, 0) == TILE_WALL);
    CHECK(map_get(&m, 3, 1) == TILE_WALL);
    CHECK(!map_walkable(&m, 0, -1));
    CHECK(!map_transparent(&m, 0, 3));
    CHECK(map_walkable(&m, 1, 1) && map_transparent(&m, 1, 1));
    map_free(&m);
}

static void seen_cells_stay_explored(void)
{
    Map m;
    map_init(&m, 3, 3);
    map_mark_seen(&m, 1, 2);
    CHECK(map_is_visible(&m, 1, 2) && map_is_explored(&m, 1, 2));
    map_clear_visible(&m);
    CHECK(!map_is_visible(&m, 1, 2) && map_is_explored(&m, 1, 2));
    map_mark_seen(&m, 7, 7); /* outside: no effect, no crash */
    map_free(&m);
}

void run_map_tests(void)
{
    RUN(new_map_is_solid_and_unseen);
    RUN(outside_reads_as_wall_and_writes_are_ignored);
    RUN(seen_cells_stay_explored);
}
