#include "core/monster.h"

/* Classic fantasy dungeon, weakest first. Stats: hp, to_hit, defense,
 * damage min-max, speed; then depths, flags, experience for a kill. */
const MonsterInfo MONSTERS[MON_COUNT] = {
    [MON_RAT] = {"rat", "bites", 'r', LOOK_BEAST, {3, 0, 0, 1, 3, 100}, 1, 3, 0, 1},
    [MON_BAT] =
        {"bat", "bites", 'b', LOOK_BEAST, {4, 0, 2, 1, 2, 150}, 1, 4, MON_ERRATIC | MON_AWAKE, 2},
    [MON_KOBOLD] = {"kobold", "hits", 'k', LOOK_HUMANOID, {6, 0, 1, 1, 4, 100}, 1, 4, 0, 3},
    [MON_GOBLIN] = {"goblin", "hits", 'g', LOOK_HUMANOID, {8, 1, 1, 1, 6, 100}, 2, 6, 0, 5},
    [MON_SPIDER] = {"giant spider", "bites", 'S', LOOK_BEAST, {12, 2, 2, 2, 6, 120}, 3, 7, 0, 9},
    [MON_ORC] = {"orc", "hits", 'o', LOOK_HUMANOID, {14, 2, 3, 2, 8, 100}, 4, 8, 0, 12},
    [MON_SKELETON] = {"skeleton", "hits", 'z', LOOK_UNDEAD, {16, 3, 3, 2, 8, 90}, 5, 9, 0, 16},
    [MON_OGRE] = {"ogre", "clubs", 'O', LOOK_BRUTE, {26, 3, 2, 3, 12, 80}, 6, 10, 0, 25},
    [MON_TROLL] = {"troll", "claws", 'T', LOOK_BRUTE, {34, 4, 4, 4, 12, 100}, 7, 10, 0, 40},
    [MON_WRAITH] =
        {"wraith", "touches", 'W', LOOK_UNDEAD, {30, 5, 5, 3, 10, 110}, 8, 10, MON_AWAKE, 50},
};

const MonsterInfo *monster_info(const Monster *m)
{
    return &MONSTERS[m->kind];
}

MonsterKind monster_pick_kind(Rng *rng, int depth)
{
    MonsterKind living[MON_COUNT];
    int count = 0;
    for (int k = 0; k < MON_COUNT; k++)
        if (MONSTERS[k].min_depth <= depth && depth <= MONSTERS[k].max_depth)
            living[count++] = (MonsterKind)k;
    if (count == 0) /* deeper than any range: the deepest kind */
        return (MonsterKind)(MON_COUNT - 1);
    return living[rng_range(rng, 0, count - 1)];
}

Monster monster_make(MonsterKind kind, Vec2 pos, Rng *rng)
{
    const MonsterInfo *info = &MONSTERS[kind];
    return (Monster){
        .kind = kind,
        .pos = pos,
        .hp = info->stats.hp_max,
        .energy = rng_range(rng, 0, 99),
        .awake = (info->flags & MON_AWAKE) || rng_chance(rng, 25),
    };
}

const char *monster_article(MonsterKind kind)
{
    switch (MONSTERS[kind].name[0])
    {
    case 'a':
    case 'e':
    case 'i':
    case 'o':
    case 'u':
        return "an";
    default:
        return "a";
    }
}
