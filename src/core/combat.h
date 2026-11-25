/* Fighting numbers and dice, the same for the player and monsters. */
#ifndef ROGUEC_COMBAT_H
#define ROGUEC_COMBAT_H

#include <stdbool.h>

#include "core/rng.h"

typedef struct
{
    int hp_max;
    int to_hit;  /* +5 % chance to hit per point */
    int defense; /* -5 % chance to be hit per point */
    int dmg_min;
    int dmg_max;
    int speed; /* energy per tick: 100 acts once a tick, 150 three times in two */
} Stats;

/* Percent chance that `attacker` hits `defender`: 60 + 5 * (to_hit - defense),
 * kept within 5..95 so nothing is certain. */
int combat_hit_chance(const Stats *attacker, const Stats *defender);
/* Rolls an attack: 0 for a miss, else the damage (dmg_min..dmg_max, at least 1). */
int combat_attack(Rng *rng, const Stats *attacker, const Stats *defender);

#endif
