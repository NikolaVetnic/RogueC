/* Items: kinds (tables), instances, and what the player knows about them. */
#ifndef ROGUEC_ITEM_H
#define ROGUEC_ITEM_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

typedef enum
{
    ITEM_GOLD,
    ITEM_FOOD,
    ITEM_POTION,
    ITEM_SCROLL,
    ITEM_WEAPON,
    ITEM_ARMOUR,
    ITEM_GOAL, /* the one thing to bring back up: GOAL_NAME */
    ITEM_CLASS_COUNT
} ItemClass;

#define GOAL_NAME "the Sunstone"

typedef enum
{
    POTION_HEALING,
    POTION_EXTRA_HEALING,
    POTION_POISON,
    POTION_SPEED,
    POTION_CONFUSION,
    POTION_BLINDNESS,
    POTION_COUNT
} PotionKind;

typedef enum
{
    SCROLL_IDENTIFY,
    SCROLL_MAGIC_MAPPING,
    SCROLL_TELEPORT,
    SCROLL_ENCHANT_WEAPON,
    SCROLL_ENCHANT_ARMOUR,
    SCROLL_AGGRAVATE,
    SCROLL_COUNT
} ScrollKind;

typedef enum
{
    WEAPON_DAGGER,
    WEAPON_SHORT_SWORD,
    WEAPON_MACE,
    WEAPON_LONG_SWORD,
    WEAPON_BATTLE_AXE,
    WEAPON_COUNT
} WeaponKind;

typedef enum
{
    ARMOUR_LEATHER,
    ARMOUR_STUDDED,
    ARMOUR_SCALE,
    ARMOUR_CHAIN,
    ARMOUR_PLATE,
    ARMOUR_COUNT
} ArmourKind;

typedef struct
{
    const char *name;
    int to_hit;
    int dmg_min;
    int dmg_max;
    int min_depth; /* found from this depth on */
} WeaponInfo;

typedef struct
{
    const char *name;
    int defense;
    int min_depth;
} ArmourInfo;

extern const char *const POTION_NAMES[POTION_COUNT]; /* "healing" */
extern const char *const SCROLL_NAMES[SCROLL_COUNT];
extern const WeaponInfo WEAPONS[WEAPON_COUNT];
extern const ArmourInfo ARMOURS[ARMOUR_COUNT];

typedef struct
{
    ItemClass cls;
    int kind;    /* PotionKind, ScrollKind, WeaponKind, ArmourKind; 0 for gold and food */
    int count;   /* stack size; gold: the amount */
    int enchant; /* weapons and armour: bonus to hit and damage, or to defense */
    bool enchant_known;
    bool equipped; /* wielded or worn */
} Item;

enum
{
    TITLE_SIZE = 24,
    POTION_LOOK_COUNT = 12, /* colours a potion can have */
};

/* What potions look like and scrolls are titled in this game, and which
 * kinds the player has identified. */
typedef struct
{
    uint8_t potion_look[POTION_COUNT]; /* index into the colour list */
    char scroll_title[SCROLL_COUNT][TITLE_SIZE];
    bool potion_known[POTION_COUNT];
    bool scroll_known[SCROLL_COUNT];
} ItemLore;

/* Shuffles looks and makes up titles: different every seed. */
void item_lore_init(ItemLore *lore, Rng *rng);
bool item_kind_known(const ItemLore *lore, const Item *it);
void item_identify(ItemLore *lore, Item *it);

/* Potions, scrolls and food stack; weapons and armour don't. */
bool item_stacks(const Item *a, const Item *b);
uint32_t item_glyph(ItemClass cls);
/* A random item (not gold) for `depth`. */
Item item_random(Rng *rng, int depth);
Item item_gold(Rng *rng, int depth);

/* What the player calls it: "2 food rations", "a fizzy potion",
 * "a potion of healing", "a +1 mace (wielded)". */
void item_name(const ItemLore *lore, const Item *it, char *out, int size);

#endif
