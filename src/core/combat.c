#include "core/combat.h"

int combat_hit_chance(const Stats *attacker, const Stats *defender)
{
    int pct = 60 + 5 * (attacker->to_hit - defender->defense);
    return pct < 5 ? 5 : pct > 95 ? 95 : pct;
}

int combat_attack(Rng *rng, const Stats *attacker, const Stats *defender)
{
    if (!rng_chance(rng, combat_hit_chance(attacker, defender)))
        return 0;
    int dmg = rng_range(rng, attacker->dmg_min, attacker->dmg_max);
    return dmg < 1 ? 1 : dmg;
}
