#include "core/game.h"

#include <stdlib.h>

#include "core/fov.h"

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

static void free_levels(Level *levels, int count)
{
    for (int i = 0; i < count; i++)
        level_free(&levels[i]);
    free(levels);
}

/* Moves the player to `pos` on level `depth` and looks around. */
static void enter_level(Game *g, int depth, Vec2 pos)
{
    g->depth = depth;
    g->map = &g->levels[depth - 1].map;
    g->player.pos = pos;
    fov_compute(g->map, g->player.pos, g->fov_radius);
}

bool game_init(Game *g, uint64_t seed)
{
    *g = (Game){.seed = seed, .fov_radius = FOV_RADIUS, .running = true};
    rng_seed(&g->rng, seed);

    g->levels = calloc(DUNGEON_DEPTH, sizeof *g->levels);
    if (!g->levels)
        return false;
    for (int d = 1; d <= DUNGEON_DEPTH; d++)
        if (!level_generate(&g->levels[d - 1], seed, d, DUNGEON_DEPTH, MAP_WIDTH, MAP_HEIGHT))
        {
            free_levels(g->levels, d - 1);
            g->levels = NULL;
            return false;
        }

    g->player = (Player){.hp = PLAYER_HP, .hp_max = PLAYER_HP};
    enter_level(g, 1, g->levels[0].stairs_up);
    msglog_clear(&g->log);
    msglog_add(&g->log, g->turn, "You enter the dungeon. Press ? for help.");
    return true;
}

void game_free(Game *g)
{
    if (g->levels)
        free_levels(g->levels, DUNGEON_DEPTH);
    g->levels = NULL;
    g->map = NULL;
}

static bool try_move(Game *g, Vec2 dir)
{
    Vec2 to = vec2_add(g->player.pos, dir);
    if (!map_walkable(g->map, to.x, to.y))
    {
        msglog_add(&g->log, g->turn, "Rock blocks the way.");
        return false;
    }
    g->player.pos = to;
    return true;
}

/* Takes the stairs under the player, if they go `dir` (+1 down, -1 up). */
static bool take_stairs(Game *g, int dir)
{
    Tile here = map_get(g->map, g->player.pos.x, g->player.pos.y);
    if (here != (dir > 0 ? TILE_STAIRS_DOWN : TILE_STAIRS_UP))
    {
        msglog_add(&g->log, g->turn, "There are no stairs %s here.", dir > 0 ? "down" : "up");
        return false;
    }

    int depth = g->depth + dir;
    const Level *to = &g->levels[depth - 1];
    enter_level(g, depth, dir > 0 ? to->stairs_up : to->stairs_down);
    msglog_add(&g->log, g->turn, dir > 0 ? "You go down to depth %d." : "You climb up to depth %d.",
               depth);
    return true;
}

bool game_step(Game *g, Action a)
{
    bool used_turn = false;
    if (a != ACTION_NONE)
        g->fresh_serial = msglog_next_serial(&g->log); /* earlier messages are now old */

    if (a >= ACTION_MOVE_N && a <= ACTION_MOVE_NW)
        used_turn = try_move(g, MOVE_DIR[a - ACTION_MOVE_N]);
    else if (a == ACTION_WAIT)
        used_turn = true;
    else if (a == ACTION_DESCEND || a == ACTION_ASCEND)
        used_turn = take_stairs(g, a == ACTION_DESCEND ? 1 : -1);
    else if (a == ACTION_QUIT)
        g->running = false;

    if (used_turn)
    {
        g->turn++;
        fov_compute(g->map, g->player.pos, g->fov_radius);
    }
    return used_turn;
}
