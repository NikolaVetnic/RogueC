/* Game state and rules. No input or drawing here: frontends turn keys into
 * Actions, call game_step, and draw with render_game. */
#ifndef ROGUEC_GAME_H
#define ROGUEC_GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "core/combat.h"
#include "core/item.h"
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
    ACTION_PICK_UP,
    ACTION_USE,   /* an item in the pack: eat, drink, read, wield, wear or take off */
    ACTION_DROP,  /* an item in the pack */
    ACTION_RUN_N, /* run: move until something interesting happens */
    ACTION_RUN_NE,
    ACTION_RUN_E,
    ACTION_RUN_SE,
    ACTION_RUN_S,
    ACTION_RUN_SW,
    ACTION_RUN_W,
    ACTION_RUN_NW,
    ACTION_TRAVEL, /* walk to a cell over known ground: game_travel */
    ACTION_QUIT,
} Action;

enum
{
    PACK_SIZE = 20, /* items in the pack, letters a-t */
    NUTRITION_START = 1300,
    NUTRITION_MAX = 2000,
    NUTRITION_RATION = 900,
    HUNGRY_BELOW = 300,
    WEAK_BELOW = 100, /* and no healing */
    STARVE_TICKS = 4, /* at 0 nutrition: 1 HP lost this often */
};

typedef enum
{
    HUNGER_FED,
    HUNGER_HUNGRY,
    HUNGER_WEAK,
    HUNGER_STARVING,
} Hunger;

typedef enum
{
    DEATH_MONSTER,
    DEATH_STARVATION,
} DeathCause;

typedef struct
{
    Vec2 pos;
    int hp;
    int energy; /* acts at ACT_COST */
    Stats base; /* without gear and effects: see game_player_stats */
    int gold;
    int level; /* experience level, 1..MAX_LEVEL */
    int xp;
    int nutrition; /* -1 per tick; food adds */
    int haste;     /* ticks left of each effect */
    int confused;
    int blind;
    Item pack[PACK_SIZE];
    int pack_count;
} Player;

enum
{
    DUNGEON_DEPTH = 10, /* levels */
    MAP_WIDTH = 100,    /* every level's size */
    MAP_HEIGHT = 42,
    ACT_COST = 100,         /* energy an action takes */
    REGEN_TURNS = 10,       /* the player heals 1 HP this often */
    REST_MAX = 500,         /* turns one rest can last */
    MAX_LEVEL = 12,         /* experience levels */
    LEVEL_HP = 6,           /* max HP gained per level */
    WANDER_ODDS = 300,      /* default Game.wander_odds */
    GOAL_WANDER_ODDS = 100, /* ... once the goal is taken: the dungeon stirs */
    RUN_MAX = 200,          /* steps one run can last */
};

/* A run or a travel in progress (not saved). */
typedef struct
{
    bool active;
    bool travel; /* walking to `target`, not running straight */
    Vec2 target;
    int dir;   /* 0-7, clockwise from north; travel: the last step's */
    int steps; /* taken so far */
    bool room; /* the last step ended in the open, not in a corridor */
    int sides; /* in the open: which cells beside the path were floor */
} Run;

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
    bool dead;
    bool won; /* climbed out of depth 1 with the goal */
    int deepest;
    DeathCause death;
    MonsterKind killer; /* if death == DEATH_MONSTER */
    ItemLore lore;      /* item looks and what's identified */
    MsgLog log;
    int fresh_serial; /* messages from this serial on came after the last action */
    Run run;
} Game;

/* Generates the dungeon from `seed` and puts the player at the entrance of
 * depth 1. Returns false if out of memory. */
bool game_init(Game *g, uint64_t seed);
void game_free(Game *g);

/* Applies one player action. Returns true if it used a turn. Turn-based: the
 * world then runs until the player can act again. Real time: nothing happens
 * if the player isn't ready (game_ready), and the world runs on game_tick. */
bool game_step(Game *g, Action a);
/* game_step for actions on a pack item (ACTION_USE, ACTION_DROP). */
bool game_step_item(Game *g, Action a, int item);
/* Walks to `to` over explored floor, step by step until there or until
 * something happens (as a run stops). With a monster in view or confused it is
 * one step. Returns true if it used a turn; says so if there is no known way. */
bool game_travel(Game *g, Vec2 to);
/* The player has the energy to act. */
bool game_ready(const Game *g);
/* Dead or won: nothing more happens. */
bool game_over(const Game *g);
/* Experience needed for `level` (1..MAX_LEVEL). */
int game_xp_needed(int level);
bool game_has_goal(const Game *g);
/* One tick of the world: monsters act, the player gains energy and heals. */
void game_tick(Game *g);
void game_set_realtime(Game *g, bool on);
/* A run or travel is under way. Turn-based ones finish inside game_step and
 * game_travel; in real time call game_run_continue whenever the player is ready. */
bool game_run_active(const Game *g);
/* Real time: the run's next step, if the player is ready. */
void game_run_continue(Game *g);
/* Ends a run (a key was pressed). */
void game_run_stop(Game *g);
Level *game_level(Game *g);
/* The player's stats with gear and effects. */
Stats game_player_stats(const Game *g);
Hunger game_hunger(const Game *g);

/* For ai.c: monster m (on the current level) attacks the player. */
void game_monster_attack(Game *g, Monster *m);
/* For inventory.c: recompute the view and announce monsters in it. */
void game_look(Game *g);
/* For inventory.c: the player dies of `cause` (not a monster). */
void game_kill_player(Game *g, DeathCause cause);

#endif
