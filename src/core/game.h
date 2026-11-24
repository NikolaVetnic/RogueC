/* Game state and rules. No input or drawing here: frontends turn keys into
 * Actions, call game_step, and draw with render_game. */
#ifndef ROGUEC_GAME_H
#define ROGUEC_GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "core/map.h"
#include "core/rng.h"
#include "core/vec2.h"

typedef enum
{
    ACTION_NONE,
    ACTION_MOVE_N,
    ACTION_MOVE_NE,
    ACTION_MOVE_E,
    ACTION_MOVE_SE,
    ACTION_MOVE_S,
    ACTION_MOVE_SW,
    ACTION_MOVE_W,
    ACTION_MOVE_NW,
    ACTION_WAIT,
    ACTION_QUIT,
} Action;

typedef struct
{
    Vec2 pos;
    int hp;
    int hp_max;
} Player;

typedef struct
{
    uint64_t seed;
    Rng rng;
    Map map;
    Player player;
    int turn;
    int fov_radius;
    bool running;
} Game;

/* Generates the first level from `seed`. Returns false if out of memory. */
bool game_init(Game *g, uint64_t seed);
void game_free(Game *g);

/* Applies one player action. Returns true if it used a turn. */
bool game_step(Game *g, Action a);

#endif
