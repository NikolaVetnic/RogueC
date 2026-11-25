#include "core/item.h"

#include <stdio.h>
#include <string.h>

const char *const POTION_NAMES[POTION_COUNT] = {
    [POTION_HEALING] = "healing",     [POTION_EXTRA_HEALING] = "extra healing",
    [POTION_POISON] = "poison",       [POTION_SPEED] = "speed",
    [POTION_CONFUSION] = "confusion", [POTION_BLINDNESS] = "blindness",
};

const char *const SCROLL_NAMES[SCROLL_COUNT] = {
    [SCROLL_IDENTIFY] = "identify",
    [SCROLL_MAGIC_MAPPING] = "magic mapping",
    [SCROLL_TELEPORT] = "teleportation",
    [SCROLL_ENCHANT_WEAPON] = "enchant weapon",
    [SCROLL_ENCHANT_ARMOUR] = "enchant armour",
    [SCROLL_AGGRAVATE] = "aggravate monsters",
};

const WeaponInfo WEAPONS[WEAPON_COUNT] = {
    [WEAPON_DAGGER] = {"dagger", 2, 1, 4, 1},
    [WEAPON_SHORT_SWORD] = {"short sword", 1, 2, 6, 1},
    [WEAPON_MACE] = {"mace", 0, 2, 8, 2},
    [WEAPON_LONG_SWORD] = {"long sword", 0, 3, 10, 4},
    [WEAPON_BATTLE_AXE] = {"battle axe", -1, 4, 12, 6},
};

const ArmourInfo ARMOURS[ARMOUR_COUNT] = {
    [ARMOUR_LEATHER] = {"leather armour", 2, 1},
    [ARMOUR_STUDDED] = {"studded leather armour", 3, 2},
    [ARMOUR_SCALE] = {"scale mail", 4, 3},
    [ARMOUR_CHAIN] = {"chain mail", 5, 5},
    [ARMOUR_PLATE] = {"plate armour", 7, 7},
};

static const char *const POTION_LOOKS[] = {
    "red",   "blue",  "green", "murky",  "fizzy", "golden",
    "black", "milky", "smoky", "violet", "amber", "silver",
};
enum
{
    LOOK_COUNT = sizeof POTION_LOOKS / sizeof POTION_LOOKS[0],
};

static const char *const SYLLABLES[] = {
    "ab",  "zu",  "kor", "eth", "nim", "ra", "vel", "dro", "ish", "tul", "om", "pha",
    "gar", "lis", "un",  "mok", "sef", "ya", "bri", "do",  "qua", "ten", "ix", "har",
};

void item_lore_init(ItemLore *lore, Rng *rng)
{
    *lore = (ItemLore){0};

    uint8_t order[LOOK_COUNT];
    for (int i = 0; i < LOOK_COUNT; i++)
        order[i] = (uint8_t)i;
    for (int i = LOOK_COUNT - 1; i > 0; i--) /* Fisher-Yates shuffle */
    {
        int j = rng_range(rng, 0, i);
        uint8_t t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (int k = 0; k < POTION_COUNT; k++)
        lore->potion_look[k] = order[k];

    int n_syl = (int)(sizeof SYLLABLES / sizeof SYLLABLES[0]);
    for (int k = 0; k < SCROLL_COUNT; k++)
    {
        char *t = lore->scroll_title[k];
        t[0] = '\0';
        int words = rng_range(rng, 1, 2);
        for (int w = 0; w < words; w++)
        {
            if (w > 0)
                strcat(t, " ");
            int syl = rng_range(rng, 1, 3);
            for (int s = 0; s < syl; s++)
                strcat(t, SYLLABLES[rng_range(rng, 0,
                                              n_syl - 1)]); /* at most 2 x 3 x 3 + 1 < TITLE_SIZE */
        }
        for (char *c = t; *c; c++)
            *c = (char)(*c - 'a' + 'A');
    }
}

bool item_kind_known(const ItemLore *lore, const Item *it)
{
    if (it->cls == ITEM_POTION)
        return lore->potion_known[it->kind];
    if (it->cls == ITEM_SCROLL)
        return lore->scroll_known[it->kind];
    return true;
}

void item_identify(ItemLore *lore, Item *it)
{
    if (it->cls == ITEM_POTION)
        lore->potion_known[it->kind] = true;
    else if (it->cls == ITEM_SCROLL)
        lore->scroll_known[it->kind] = true;
    it->enchant_known = true;
}

bool item_stacks(const Item *a, const Item *b)
{
    return a->cls == b->cls && a->kind == b->kind &&
           (a->cls == ITEM_FOOD || a->cls == ITEM_POTION || a->cls == ITEM_SCROLL);
}

uint32_t item_glyph(ItemClass cls)
{
    static const char GLYPHS[ITEM_CLASS_COUNT] = {'$', '%', '!', '?', ')', '['};
    return (uint32_t)GLYPHS[cls];
}

/* -1..+2 mostly 0; deeper a little better. */
static int random_enchant(Rng *rng, int depth)
{
    int roll = rng_range(rng, 0, 99);
    if (roll < 15)
        return -1;
    if (roll < 70 - 2 * depth)
        return 0;
    if (roll < 92)
        return 1;
    return 2;
}

Item item_random(Rng *rng, int depth)
{
    int roll = rng_range(rng, 0, 99);
    Item it = {.count = 1};
    if (roll < 20)
        it.cls = ITEM_FOOD;
    else if (roll < 50)
    {
        it.cls = ITEM_POTION;
        it.kind = rng_range(rng, 0, POTION_COUNT - 1);
    }
    else if (roll < 80)
    {
        it.cls = ITEM_SCROLL;
        it.kind = rng_range(rng, 0, SCROLL_COUNT - 1);
    }
    else if (roll < 90)
    {
        it.cls = ITEM_WEAPON;
        do
            it.kind = rng_range(rng, 0, WEAPON_COUNT - 1);
        while (WEAPONS[it.kind].min_depth > depth);
        it.enchant = random_enchant(rng, depth);
    }
    else
    {
        it.cls = ITEM_ARMOUR;
        do
            it.kind = rng_range(rng, 0, ARMOUR_COUNT - 1);
        while (ARMOURS[it.kind].min_depth > depth);
        it.enchant = random_enchant(rng, depth);
    }
    return it;
}

Item item_gold(Rng *rng, int depth)
{
    return (Item){.cls = ITEM_GOLD, .count = rng_range(rng, 2 + 3 * depth, 10 + 12 * depth)};
}

void item_name(const ItemLore *lore, const Item *it, char *out, int size)
{
    size_t n = (size_t)size;
    bool many = it->count > 1;
    char count[16];
    snprintf(count, sizeof count, "%d", it->count);
    const char *a = many ? count : "a";
    char ench[8] = "";
    if (it->enchant_known)
        snprintf(ench, sizeof ench, "%+d ", it->enchant);

    switch (it->cls)
    {
    case ITEM_GOLD:
        snprintf(out, n, "%d gold", it->count);
        break;
    case ITEM_FOOD:
        snprintf(out, n, "%s food ration%s", a, many ? "s" : "");
        break;
    case ITEM_POTION:
        if (lore->potion_known[it->kind])
            snprintf(out, n, "%s potion%s of %s", a, many ? "s" : "", POTION_NAMES[it->kind]);
        else
        {
            const char *look = POTION_LOOKS[lore->potion_look[it->kind]];
            bool vowel = strchr("aeiou", look[0]) != NULL;
            snprintf(out, n, "%s %s potion%s",
                     many    ? count
                     : vowel ? "an"
                             : "a",
                     look, many ? "s" : "");
        }
        break;
    case ITEM_SCROLL:
        if (lore->scroll_known[it->kind])
            snprintf(out, n, "%s scroll%s of %s", a, many ? "s" : "", SCROLL_NAMES[it->kind]);
        else
            snprintf(out, n, "%s scroll%s titled \"%s\"", a, many ? "s" : "",
                     lore->scroll_title[it->kind]);
        break;
    case ITEM_WEAPON:
        snprintf(out, n, "a %s%s%s", ench, WEAPONS[it->kind].name,
                 it->equipped ? " (wielded)" : "");
        break;
    case ITEM_ARMOUR:
        snprintf(out, n, "%s%s%s", ench, ARMOURS[it->kind].name, it->equipped ? " (worn)" : "");
        break;
    default:
        snprintf(out, n, "something");
        break;
    }
}
