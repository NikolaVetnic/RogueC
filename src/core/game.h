/* Game state and rules. No input or drawing here: frontends turn keys into
 * Actions, call game_step, and draw with render_game. */
#ifndef ROGUEC_GAME_H
#define ROGUEC_GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "core/level.h"
#include "core/map.h"
#include "core/msglog.h"
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
    ACTION_DESCEND,
    ACTION_ASCEND,
    ACTION_QUIT,
} Action;

typedef struct
{
    Vec2 pos;
    int hp;
    int hp_max;
} Player;

enum
{
    DUNGEON_DEPTH = 10, /* levels */
};

/* Owns its levels: free with game_free. Copying a Game shares them. */
typedef struct
{
    uint64_t seed;
    Rng rng;
    Level *levels; /* DUNGEON_DEPTH of them, all generated at the start */
    int depth;     /* current level, 1 = top */
    Map *map;      /* the current level's map */
    Player player;
    int turn;
    int fov_radius;
    bool running;
    MsgLog log;
    int fresh_serial; /* messages from this serial on came after the last action */
} Game;

/* Generates the dungeon from `seed` and puts the player at the entrance of
 * depth 1. Returns false if out of memory. */
bool game_init(Game *g, uint64_t seed);
void game_free(Game *g);

/* Applies one player action. Returns true if it used a turn. */
bool game_step(Game *g, Action a);

#endif
