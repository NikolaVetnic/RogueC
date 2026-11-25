/* Game state and rules. No input or drawing here: frontends turn keys into
 * Actions, call game_step, and draw with render_game. */
#ifndef ROGUEC_GAME_H
#define ROGUEC_GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "core/combat.h"
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
    ACTION_REST, /* wait until healed or a monster shows up */
    ACTION_DESCEND,
    ACTION_ASCEND,
    ACTION_QUIT,
} Action;

typedef struct
{
    Vec2 pos;
    int hp;
    int energy; /* acts at ACT_COST */
    Stats stats;
} Player;

enum
{
    DUNGEON_DEPTH = 10, /* levels */
    ACT_COST = 100,     /* energy an action takes */
    REGEN_TURNS = 10,   /* the player heals 1 HP this often */
    REST_MAX = 500,     /* turns one rest can last */
};

/* Owns its levels: free with game_free. Copying a Game shares them. */
typedef struct
{
    uint64_t seed;
    Rng rng;
    Level *levels;    /* DUNGEON_DEPTH of them, all generated at the start */
    int depth;        /* current level, 1 = top */
    Map *map;         /* the current level's map */
    int *player_dist; /* steps to the player from each cell, refreshed every tick */
    int wander_odds;  /* a monster wanders in on 1 tick in this many; 0: never */
    Player player;
    int turn;
    int fov_radius;
    bool running;
    bool realtime; /* the world runs on a clock (game_tick), not on the player's moves */
    bool dead;     /* the player died; killer says by what */
    MonsterKind killer;
    MsgLog log;
    int fresh_serial; /* messages from this serial on came after the last action */
} Game;

/* Generates the dungeon from `seed` and puts the player at the entrance of
 * depth 1. Returns false if out of memory. */
bool game_init(Game *g, uint64_t seed);
void game_free(Game *g);

/* Applies one player action. Returns true if it used a turn. Turn-based: the
 * world then runs until the player can act again. Real time: nothing happens
 * if the player isn't ready (game_ready), and the world runs on game_tick. */
bool game_step(Game *g, Action a);
/* The player has the energy to act. */
bool game_ready(const Game *g);
/* One tick of the world: monsters act, the player gains energy and heals. */
void game_tick(Game *g);
void game_set_realtime(Game *g, bool on);
Level *game_level(Game *g);

/* For ai.c: monster m (on the current level) attacks the player. */
void game_monster_attack(Game *g, Monster *m);

#endif
