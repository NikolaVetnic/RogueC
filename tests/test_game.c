#include "test.h"

#include <string.h>

#include "core/game.h"

static void player_starts_on_floor_and_sees_around(void)
{
    for (uint64_t seed = 0; seed < 50; seed++)
    {
        Game g;
        CHECK(game_init(&g, seed));
        CHECK(map_get(&g.map, g.player.pos.x, g.player.pos.y) == TILE_FLOOR);
        CHECK(map_is_visible(&g.map, g.player.pos.x, g.player.pos.y));
        CHECK(g.running && g.turn == 0);
        game_free(&g);
    }
}

/* Walls stop movement without using a turn; floor moves use one. */
static void moves_respect_walls(void)
{
    Game g;
    game_init(&g, 3);
    map_fill(&g.map, TILE_WALL);
    g.player.pos = vec2(10, 10);
    map_set(&g.map, 10, 10, TILE_FLOOR);
    map_set(&g.map, 11, 9, TILE_FLOOR); /* north-east */

    CHECK(!game_step(&g, ACTION_MOVE_N));
    CHECK(vec2_eq(g.player.pos, vec2(10, 10)) && g.turn == 0);

    CHECK(game_step(&g, ACTION_MOVE_NE));
    CHECK(vec2_eq(g.player.pos, vec2(11, 9)) && g.turn == 1);

    CHECK(game_step(&g, ACTION_WAIT) && g.turn == 2);
    CHECK(!game_step(&g, ACTION_NONE) && g.turn == 2);

    game_step(&g, ACTION_QUIT);
    CHECK(!g.running);
    game_free(&g);
}

/* Same seed and same inputs give the same game. */
static void games_are_deterministic(void)
{
    static const Action script[] = {ACTION_MOVE_N,  ACTION_MOVE_E, ACTION_MOVE_E, ACTION_MOVE_S,
                                    ACTION_MOVE_SW, ACTION_WAIT,   ACTION_MOVE_W, ACTION_MOVE_NW};
    Game a, b;
    game_init(&a, 99);
    game_init(&b, 99);
    for (size_t i = 0; i < sizeof script / sizeof script[0]; i++)
    {
        game_step(&a, script[i]);
        game_step(&b, script[i]);
    }
    CHECK(vec2_eq(a.player.pos, b.player.pos));
    CHECK(a.turn == b.turn);
    size_t n = (size_t)a.map.w * (size_t)a.map.h;
    CHECK(memcmp(a.map.tiles, b.map.tiles, n) == 0);
    CHECK(memcmp(a.map.explored, b.map.explored, n * sizeof *a.map.explored) == 0);
    game_free(&a);
    game_free(&b);
}

void run_game_tests(void)
{
    RUN(player_starts_on_floor_and_sees_around);
    RUN(moves_respect_walls);
    RUN(games_are_deterministic);
}
