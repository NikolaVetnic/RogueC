#include "core/game.h"

#include "core/fov.h"
#include "core/gen_cave.h"

enum
{
    MAP_WIDTH = 100,
    MAP_HEIGHT = 42,
    FOV_RADIUS = 10,
    PLAYER_HP = 20,
};

/* Indexed by action - ACTION_MOVE_N, clockwise from north. */
static const Vec2 MOVE_DIR[8] = {
    {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1},
};

/* A uniformly random floor cell; the map has at least one. */
static Vec2 random_floor(const Map *m, Rng *rng)
{
    for (;;)
    {
        int x = rng_range(rng, 0, m->w - 1);
        int y = rng_range(rng, 0, m->h - 1);
        if (map_get(m, x, y) == TILE_FLOOR)
            return vec2(x, y);
    }
}

bool game_init(Game *g, uint64_t seed)
{
    *g = (Game){.seed = seed, .fov_radius = FOV_RADIUS, .running = true};
    rng_seed(&g->rng, seed);

    if (!map_init(&g->map, MAP_WIDTH, MAP_HEIGHT))
        return false;
    if (gen_cave(&g->map, &g->rng, &CAVE_DEFAULTS) <= 0)
    {
        map_free(&g->map);
        return false;
    }

    g->player =
        (Player){.pos = random_floor(&g->map, &g->rng), .hp = PLAYER_HP, .hp_max = PLAYER_HP};
    fov_compute(&g->map, g->player.pos, g->fov_radius);
    return true;
}

void game_free(Game *g)
{
    map_free(&g->map);
}

static bool try_move(Game *g, Vec2 dir)
{
    Vec2 to = vec2_add(g->player.pos, dir);
    if (!map_walkable(&g->map, to.x, to.y))
        return false;
    g->player.pos = to;
    return true;
}

bool game_step(Game *g, Action a)
{
    bool used_turn = false;

    if (a >= ACTION_MOVE_N && a <= ACTION_MOVE_NW)
        used_turn = try_move(g, MOVE_DIR[a - ACTION_MOVE_N]);
    else if (a == ACTION_WAIT)
        used_turn = true;
    else if (a == ACTION_QUIT)
        g->running = false;

    if (used_turn)
    {
        g->turn++;
        fov_compute(&g->map, g->player.pos, g->fov_radius);
    }
    return used_turn;
}
