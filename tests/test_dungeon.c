#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "core/gen_rooms.h"
#include "core/level.h"
#include "core/path.h"

enum
{
    W = 100,
    H = 42,
};

static int dist[W * H];

/* Floor-like cells (walkable) form one 4-connected region. */
static bool connected4(const Map *m)
{
    static int queue[W * H];
    static bool seen[W * H];
    int n = m->w * m->h, start = -1, walkable = 0;
    memset(seen, 0, sizeof seen);
    for (int i = 0; i < n; i++)
        if (map_walkable(m, i % m->w, i / m->w))
        {
            walkable++;
            if (start < 0)
                start = i;
        }
    if (start < 0)
        return false;

    int head = 0, tail = 0, reached = 0;
    queue[tail++] = start;
    seen[start] = true;
    while (head < tail)
    {
        int i = queue[head++], x = i % m->w, y = i / m->w;
        reached++;
        const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++)
        {
            int nx = x + dx[k], ny = y + dy[k], j = ny * m->w + nx;
            if (map_walkable(m, nx, ny) && !seen[j])
            {
                seen[j] = true;
                queue[tail++] = j;
            }
        }
    }
    return reached == walkable;
}

static bool border_is_wall(const Map *m)
{
    for (int x = 0; x < m->w; x++)
        if (map_get(m, x, 0) != TILE_WALL || map_get(m, x, m->h - 1) != TILE_WALL)
            return false;
    for (int y = 0; y < m->h; y++)
        if (map_get(m, 0, y) != TILE_WALL || map_get(m, m->w - 1, y) != TILE_WALL)
            return false;
    return true;
}

/* ---- path ----------------------------------------------------------------- */

static void distances_count_diagonal_steps(void)
{
    Map m;
    map_init(&m, 12, 8);
    for (int y = 1; y < 7; y++)
        for (int x = 1; x < 11; x++)
            map_set(&m, x, y, TILE_FLOOR);
    map_set(&m, 5, 1, TILE_WALL); /* a wall across, open at the bottom */
    map_set(&m, 5, 2, TILE_WALL);
    map_set(&m, 5, 3, TILE_WALL);
    map_set(&m, 5, 4, TILE_WALL);
    map_set(&m, 5, 5, TILE_WALL);

    CHECK(path_distances(&m, vec2(1, 1), dist));
    CHECK(dist[1 * 12 + 1] == 0);
    CHECK(dist[4 * 12 + 4] == 3);  /* diagonal: max(dx, dy) */
    CHECK(dist[1 * 12 + 6] == 10); /* around the wall: 5 steps to (5, 6), 5 up */
    CHECK(dist[0] == -1);          /* wall */
    CHECK(dist[1 * 12 + 5] == -1);

    CHECK(path_distances(&m, vec2(0, 0), dist)); /* from a wall: nothing reached */
    int reached = 0;
    for (int i = 0; i < 12 * 8; i++)
        reached += dist[i] >= 0;
    CHECK(reached == 0);
    map_free(&m);
}

/* ---- rooms ---------------------------------------------------------------- */

static void rooms_are_connected_and_bordered(void)
{
    Map m;
    map_init(&m, W, H);
    for (uint64_t seed = 0; seed < 200; seed++)
    {
        Rng rng;
        rng_seed(&rng, seed);
        int rooms = gen_rooms(&m, &rng, &ROOM_DEFAULTS);
        CHECK(rooms >= 2 && rooms <= ROOM_DEFAULTS.max_rooms);
        CHECK(connected4(&m));
        CHECK(border_is_wall(&m));
    }
    map_free(&m);
}

static void rooms_need_room(void)
{
    Map m;
    map_init(&m, 6, 5); /* fits one room at most */
    Rng rng;
    rng_seed(&rng, 1);
    CHECK(gen_rooms(&m, &rng, &ROOM_DEFAULTS) == 0);
    map_free(&m);
}

/* ---- levels --------------------------------------------------------------- */

static void cave_chance_by_depth(void)
{
    CHECK(level_cave_chance(1) == 0 && level_cave_chance(3) == 0);
    CHECK(level_cave_chance(4) == 25 && level_cave_chance(6) == 75);
    CHECK(level_cave_chance(7) == 100 && level_cave_chance(10) == 100);
}

static void levels_have_reachable_stairs(void)
{
    enum
    {
        BOTTOM = 10,
    };
    int caves_mid = 0;
    for (uint64_t seed = 0; seed < 30; seed++)
        for (int depth = 1; depth <= BOTTOM; depth++)
        {
            Level lv;
            CHECK(level_generate(&lv, seed, depth, BOTTOM, W, H));
            const Map *m = &lv.map;
            CHECK(connected4(m));
            CHECK(border_is_wall(m));
            if (depth <= 3)
                CHECK(!lv.caves);
            if (depth >= 7)
                CHECK(lv.caves);
            caves_mid += depth == 5 && lv.caves;

            Tile up = map_get(m, lv.stairs_up.x, lv.stairs_up.y);
            CHECK(depth == 1 ? up == TILE_FLOOR : up == TILE_STAIRS_UP);

            if (depth == BOTTOM)
            {
                CHECK(lv.stairs_down.x == -1 && lv.stairs_down.y == -1);
            }
            else
            {
                CHECK(map_get(m, lv.stairs_down.x, lv.stairs_down.y) == TILE_STAIRS_DOWN);
                path_distances(m, lv.stairs_up, dist);
                int far = 0;
                for (int i = 0; i < W * H; i++)
                    far = dist[i] > far ? dist[i] : far;
                int d = dist[lv.stairs_down.y * W + lv.stairs_down.x];
                CHECK(d > 0 && d >= far / 2);
            }
            level_free(&lv);
        }
    CHECK(caves_mid > 0 && caves_mid < 30); /* 50 %: some of each */
}

static void same_seed_and_depth_same_level(void)
{
    Level a, b, c;
    level_generate(&a, 7, 4, 10, W, H);
    level_generate(&b, 7, 4, 10, W, H);
    level_generate(&c, 7, 5, 10, W, H);
    CHECK(memcmp(a.map.tiles, b.map.tiles, W * H) == 0);
    CHECK(vec2_eq(a.stairs_up, b.stairs_up) && vec2_eq(a.stairs_down, b.stairs_down));
    CHECK(memcmp(a.map.tiles, c.map.tiles, W * H) != 0);
    level_free(&a);
    level_free(&b);
    level_free(&c);
}

/* 3 + depth monsters of the depth's kinds, on floor, away from the arrival point. */
static void levels_start_with_monsters(void)
{
    for (uint64_t seed = 0; seed < 10; seed++)
        for (int depth = 1; depth <= 10; depth++)
        {
            Level lv;
            level_generate(&lv, seed, depth, 10, W, H);
            CHECK(lv.monster_count == 3 + depth);
            path_distances(&lv.map, lv.stairs_up, dist);
            for (int i = 0; i < lv.monster_count; i++)
            {
                const Monster *m = &lv.monsters[i];
                const MonsterInfo *info = monster_info(m);
                CHECK(info->min_depth <= depth && depth <= info->max_depth);
                CHECK(map_get(&lv.map, m->pos.x, m->pos.y) == TILE_FLOOR);
                CHECK(dist[m->pos.y * W + m->pos.x] >= SPAWN_MIN_DIST);
                CHECK(level_monster_at(&lv, m->pos) == i); /* one per cell */
                CHECK(m->hp == info->stats.hp_max);
            }
            level_free(&lv);
        }
}

void run_dungeon_tests(void)
{
    RUN(levels_start_with_monsters);
    RUN(distances_count_diagonal_steps);
    RUN(rooms_are_connected_and_bordered);
    RUN(rooms_need_room);
    RUN(cave_chance_by_depth);
    RUN(levels_have_reachable_stairs);
    RUN(same_seed_and_depth_same_level);
}
