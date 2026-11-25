/* Monster kinds (a table) and monsters (instances on a level). */
#ifndef ROGUEC_MONSTER_H
#define ROGUEC_MONSTER_H

#include <stdbool.h>
#include <stdint.h>

#include "core/combat.h"
#include "core/rng.h"
#include "core/vec2.h"

typedef enum
{
    MON_RAT,
    MON_BAT,
    MON_KOBOLD,
    MON_GOBLIN,
    MON_SPIDER,
    MON_ORC,
    MON_SKELETON,
    MON_OGRE,
    MON_TROLL,
    MON_WRAITH,
    MON_COUNT
} MonsterKind;

/* Colour family; render picks the Style. */
typedef enum
{
    LOOK_BEAST,
    LOOK_HUMANOID,
    LOOK_UNDEAD,
    LOOK_BRUTE,
} MonsterLook;

enum
{
    MON_ERRATIC = 1 << 0, /* moves at random half the time */
    MON_AWAKE = 1 << 1,   /* never found asleep */
};

typedef struct
{
    const char *name; /* "giant spider" */
    const char *verb; /* "bites": "The rat bites you." */
    uint32_t glyph;
    MonsterLook look;
    Stats stats;
    int min_depth; /* lives on these depths */
    int max_depth;
    unsigned flags;
    int xp; /* experience for killing one */
} MonsterInfo;

extern const MonsterInfo MONSTERS[MON_COUNT];

typedef struct
{
    MonsterKind kind;
    Vec2 pos;
    int hp;
    int energy;   /* acts when it reaches 100 */
    bool awake;   /* asleep monsters wait to see the player */
    bool seen;    /* in the player's view at the last check */
    int track;    /* ticks it keeps hunting after losing sight of the player */
    Vec2 heading; /* wandering direction */
} Monster;

const MonsterInfo *monster_info(const Monster *m);
/* A random kind among those living at `depth`. */
MonsterKind monster_pick_kind(Rng *rng, int depth);
/* A new monster: full health, random energy so they don't all act together,
 * a quarter awake (all for MON_AWAKE kinds). */
Monster monster_make(MonsterKind kind, Vec2 pos, Rng *rng);
/* "a" or "an" for the kind's name. */
const char *monster_article(MonsterKind kind);

#endif
