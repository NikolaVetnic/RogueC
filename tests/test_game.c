#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "core/game.h"

static void player_starts_on_floor_and_sees_around(void)
{
    for (uint64_t seed = 0; seed < 50; seed++)
    {
        Game g;
        CHECK(game_init(&g, seed));
        CHECK(map_get(g.map, g.player.pos.x, g.player.pos.y) == TILE_STAIRS_UP); /* the way out */
        CHECK(map_is_visible(g.map, g.player.pos.x, g.player.pos.y));
        CHECK(g.running && g.turn == 0);
        game_free(&g);
    }
}

/* Walls stop movement without using a turn; floor moves use one. */
static void moves_respect_walls(void)
{
    Game g;
    game_init(&g, 3);
    clear_monsters(&g);
    map_fill(g.map, TILE_WALL);
    g.player.pos = vec2(10, 10);
    map_set(g.map, 10, 10, TILE_FLOOR);
    map_set(g.map, 11, 9, TILE_FLOOR); /* north-east */

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
    size_t n = (size_t)a.map->w * (size_t)a.map->h;
    CHECK(memcmp(a.map->tiles, b.map->tiles, n) == 0);
    CHECK(memcmp(a.map->explored, b.map->explored, n * sizeof *a.map->explored) == 0);
    game_free(&a);
    game_free(&b);
}

/* Messages: a welcome, a bump into rock; any action makes earlier ones old. */
static void events_are_logged(void)
{
    Game g;
    game_init(&g, 3);
    clear_monsters(&g);
    CHECK(msglog_len(&g.log) == 1);
    CHECK(msglog_get(&g.log, 0)->serial >= g.fresh_serial);

    map_fill(g.map, TILE_WALL);
    map_set(g.map, g.player.pos.x, g.player.pos.y, TILE_FLOOR);
    game_step(&g, ACTION_MOVE_E);
    game_step(&g, ACTION_MOVE_E);
    CHECK(msglog_len(&g.log) == 2);
    const Message *bump = msglog_get(&g.log, 0);
    CHECK(strcmp(bump->text, "Rock blocks the way.") == 0 && bump->count == 2);
    CHECK(bump->serial >= g.fresh_serial);                 /* fresh */
    CHECK(msglog_get(&g.log, 1)->serial < g.fresh_serial); /* the welcome is old */

    game_step(&g, ACTION_WAIT);
    CHECK(bump->serial < g.fresh_serial);
    game_free(&g);
}

#include "core/path.h"

/* Walks to the down stairs by real moves (downhill on a distance map from the
 * stairs) and takes them, all the way to the bottom: every level can be
 * descended. */
static void descend_to_the_bottom(void)
{
    static const Vec2 DIRS[8] = {{0, -1}, {1, -1}, {1, 0},  {1, 1},
                                 {0, 1},  {-1, 1}, {-1, 0}, {-1, -1}};
    for (uint64_t seed = 0; seed < 10; seed++)
    {
        Game g;
        CHECK(game_init(&g, seed));
        clear_monsters(&g);
        int *dist = malloc((size_t)g.map->w * (size_t)g.map->h * sizeof *dist);

        while (g.depth < DUNGEON_DEPTH)
        {
            Vec2 target = g.levels[g.depth - 1].stairs_down;
            path_distances(g.map, target, dist);
            for (int steps = 0; !vec2_eq(g.player.pos, target) && steps < 1000; steps++)
            {
                clear_monsters(&g); /* wanderers arrive over time; this test is about the levels */
                int here = dist[g.player.pos.y * g.map->w + g.player.pos.x];
                for (int k = 0; k < 8; k++)
                {
                    Vec2 next = vec2_add(g.player.pos, DIRS[k]);
                    if (map_in_bounds(g.map, next.x, next.y) &&
                        dist[next.y * g.map->w + next.x] == here - 1)
                    {
                        CHECK(game_step(&g, (Action)(ACTION_MOVE_N + k)));
                        break;
                    }
                }
            }
            int depth = g.depth;
            clear_monsters(&g);
            CHECK(game_step(&g, ACTION_DESCEND));
            if (g.depth != depth + 1)
                break; /* stuck: reported by the CHECK above */
            CHECK(vec2_eq(g.player.pos, g.levels[g.depth - 1].stairs_up));
        }
        CHECK(g.depth == DUNGEON_DEPTH);
        CHECK(!game_step(&g, ACTION_DESCEND)); /* the bottom has no way down */
        free(dist);
        game_free(&g);
    }
}

/* Stairs use a turn, put you on the matching stairs, and levels keep what you saw. */
static void stairs_go_both_ways_and_levels_persist(void)
{
    Game g;
    game_init(&g, 21);
    clear_monsters(&g);
    CHECK(g.depth == 1);
    CHECK(!game_step(&g, ACTION_ASCEND)); /* the way out refuses you without the goal */
    CHECK(strcmp(msglog_get(&g.log, 0)->text,
                 "Daylight above. You won't leave without the Sunstone.") == 0);

    Vec2 down = g.levels[0].stairs_down;
    g.player.pos = down;
    int explored_before = 0;
    for (int i = 0; i < g.map->w * g.map->h; i++)
        explored_before += g.map->explored[i];

    CHECK(game_step(&g, ACTION_DESCEND) && g.turn == 1);
    CHECK(g.depth == 2 && g.map == &g.levels[1].map);
    CHECK(vec2_eq(g.player.pos, g.levels[1].stairs_up));
    CHECK(strcmp(msglog_get(&g.log, 0)->text, "You go down to depth 2.") == 0);
    CHECK(map_is_visible(g.map, g.player.pos.x, g.player.pos.y));

    CHECK(game_step(&g, ACTION_ASCEND) && g.turn == 2);
    CHECK(g.depth == 1 && vec2_eq(g.player.pos, down));
    int explored_after = 0;
    for (int i = 0; i < g.map->w * g.map->h; i++)
        explored_after += g.map->explored[i];
    CHECK(explored_after >= explored_before && explored_before > 0);
    game_free(&g);
}

void run_game_tests(void)
{
    RUN(stairs_go_both_ways_and_levels_persist);
    RUN(descend_to_the_bottom);
    RUN(events_are_logged);
    RUN(player_starts_on_floor_and_sees_around);
    RUN(moves_respect_walls);
    RUN(games_are_deterministic);
}
