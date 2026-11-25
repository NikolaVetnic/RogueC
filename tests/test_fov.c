#include "test.h"

#include "core/fov.h"
#include "core/gen_cave.h"

/* A w x h room: floor inside, wall border. */
static void make_room(Map *m, int w, int h)
{
    map_init(m, w, h);
    for (int y = 1; y < h - 1; y++)
        for (int x = 1; x < w - 1; x++)
            map_set(m, x, y, TILE_FLOOR);
}

static bool in_radius(Vec2 o, int x, int y, int r)
{
    int dx = x - o.x, dy = y - o.y;
    return dx * dx + dy * dy <= r * r + r;
}

/* In an open room every cell within the radius is visible and nothing beyond it:
 * no holes (the 2022 version missed cells beyond ~16 tiles). */
static void open_room_has_no_holes(void)
{
    for (int r = 1; r <= 30; r++)
    {
        Map m;
        make_room(&m, 2 * r + 9, 2 * r + 9);
        Vec2 o = {m.w / 2, m.h / 2};
        fov_compute(&m, o, r);

        int holes = 0, extra = 0;
        for (int y = 0; y < m.h; y++)
            for (int x = 0; x < m.w; x++)
            {
                bool expect = in_radius(o, x, y, r);
                holes += expect && !map_is_visible(&m, x, y);
                extra += !expect && map_is_visible(&m, x, y);
            }
        CHECK(holes == 0);
        CHECK(extra == 0);
        map_free(&m);
    }
}

static void walls_are_lit_and_block_sight(void)
{
    Map m;
    make_room(&m, 21, 11);
    for (int y = 1; y < 10; y++) /* wall across the room at x = 12 */
        map_set(&m, 12, y, TILE_WALL);

    Vec2 o = {6, 5};
    fov_compute(&m, o, 20);
    CHECK(map_is_visible(&m, 12, 5));  /* the wall itself */
    CHECK(map_is_visible(&m, 6, 0));   /* outer wall */
    CHECK(!map_is_visible(&m, 15, 5)); /* behind the wall */
    CHECK(!map_is_explored(&m, 15, 5));
    map_free(&m);
}

static void explored_cells_are_remembered(void)
{
    Map m;
    make_room(&m, 41, 9);
    fov_compute(&m, vec2(5, 4), 5);
    CHECK(map_is_visible(&m, 8, 4));
    fov_compute(&m, vec2(35, 4), 5);
    CHECK(!map_is_visible(&m, 8, 4));
    CHECK(map_is_explored(&m, 8, 4));
    map_free(&m);
}

static int sign(int v)
{
    return (v > 0) - (v < 0);
}

/* Unlit walls in range whose neighbour on the origin's side is visible floor:
 * the artefact the post-pass removes (~19 % of such walls without it). */
static int unlit_facing_walls(const Map *m, Vec2 o, int r)
{
    int count = 0;
    for (int y = o.y - r; y <= o.y + r; y++)
        for (int x = o.x - r; x <= o.x + r; x++)
        {
            if (!map_in_bounds(m, x, y) || !in_radius(o, x, y, r) || map_transparent(m, x, y) ||
                map_is_visible(m, x, y))
                continue;
            int sx = sign(o.x - x), sy = sign(o.y - y);
            Vec2 near[3] = {{x + sx, y}, {x, y + sy}, {x + sx, y + sy}};
            for (int i = 0; i < 3; i++)
                if (!vec2_eq(near[i], vec2(x, y)) && map_is_visible(m, near[i].x, near[i].y) &&
                    map_transparent(m, near[i].x, near[i].y))
                {
                    count++;
                    break;
                }
        }
    return count;
}

static void walls_facing_visible_floor_are_lit(void)
{
    int artefacts = 0;
    for (uint64_t seed = 1; seed <= 20; seed++)
    {
        Map m;
        Rng rng;
        rng_seed(&rng, seed);
        map_init(&m, 100, 42);
        gen_cave(&m, &rng, &CAVE_DEFAULTS);
        for (int y = 1; y < m.h; y += 4)
            for (int x = 1; x < m.w; x += 4)
                if (map_get(&m, x, y) == TILE_FLOOR)
                {
                    fov_compute(&m, vec2(x, y), 10);
                    artefacts += unlit_facing_walls(&m, vec2(x, y), 10);
                }
        map_free(&m);
    }
    CHECK(artefacts == 0);
}

/* The post-pass lights walls next to visible floor only: the outer layer of a
 * double wall stays dark. */
static void wall_behind_wall_stays_dark(void)
{
    Map m;
    make_room(&m, 21, 11);
    for (int y = 1; y < 10; y++)
    {
        map_set(&m, 12, y, TILE_WALL);
        map_set(&m, 13, y, TILE_WALL);
    }
    fov_compute(&m, vec2(6, 5), 20);
    for (int y = 1; y < 10; y++)
    {
        CHECK(map_is_visible(&m, 12, y));
        CHECK(!map_is_visible(&m, 13, y));
    }
    map_free(&m);
}

void run_fov_tests(void)
{
    RUN(open_room_has_no_holes);
    RUN(walls_are_lit_and_block_sight);
    RUN(explored_cells_are_remembered);
    RUN(walls_facing_visible_floor_are_lit);
    RUN(wall_behind_wall_stays_dark);
}
