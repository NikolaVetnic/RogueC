/* One dungeon level: its map, stairs and monsters, generated from the seed
 * and depth. */
#ifndef ROGUEC_LEVEL_H
#define ROGUEC_LEVEL_H

#include <stdbool.h>
#include <stdint.h>

#include "core/item.h"
#include "core/map.h"
#include "core/monster.h"
#include "core/vec2.h"

enum
{
    LEVEL_MAX_MONSTERS = 32,
    LEVEL_MAX_ITEMS = 48,
    SPAWN_MIN_DIST = 12, /* starting monsters are this many steps from the arrival point */
};

typedef struct
{
    Item item;
    Vec2 pos;
    bool seen; /* remembered on the map once seen */
} FloorItem;

typedef struct
{
    Map map;
    Vec2 stairs_up;   /* arrival from above; on depth 1 the entrance, plain floor */
    Vec2 stairs_down; /* (-1, -1) on the bottom level */
    bool caves;       /* cave level, else rooms and corridors */
    Monster monsters[LEVEL_MAX_MONSTERS];
    int monster_count;
    FloorItem items[LEVEL_MAX_ITEMS]; /* at most one per cell */
    int item_count;
} Level;

/* Generates the level at `depth` (1 = top) of a dungeon `bottom` levels deep,
 * w x h cells, with 3 + depth monsters, 2-4 piles of gold and 3 + depth / 3
 * other items. The same seed and depth always give
 * the same level, whatever else happened in the game. Returns false if out of
 * memory (or if the map is too small for two rooms). */
bool level_generate(Level *lv, uint64_t seed, int depth, int bottom, int w, int h);
void level_free(Level *lv);

/* Chance in percent that the level at `depth` is caves: 0 on depths 1-3, then
 * 25 more per level, 100 from depth 7. */
int level_cave_chance(int depth);

/* Index of the monster at `pos`, or -1. */
int level_monster_at(const Level *lv, Vec2 pos);
/* Returns false if the level is full. */
bool level_add_monster(Level *lv, Monster m);
/* Removes monster i; the last one takes its index. */
void level_remove_monster(Level *lv, int i);

/* Index of the item at `pos`, or -1. */
int level_item_at(const Level *lv, Vec2 pos);
/* Returns false if the level is full or something lies there already. */
bool level_add_item(Level *lv, Item it, Vec2 pos);
void level_remove_item(Level *lv, int i);

#endif
