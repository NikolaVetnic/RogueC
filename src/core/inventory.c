#include "core/inventory.h"

#include <string.h>

enum
{
    NAME_SIZE = 80,
    HEAL = 15,        /* potion of healing */
    EXTRA_HP_MAX = 2, /* potion of extra healing raises max HP */
    POISON_MIN = 3,
    POISON_MAX = 8,
    HASTE_TICKS = 25,
    CONFUSION_TICKS = 12,
    BLIND_TICKS = 20,
    AGGRAVATE_TRACK = 50,
};

int inv_add(Player *p, Item it)
{
    for (int i = 0; i < p->pack_count; i++)
        if (item_stacks(&p->pack[i], &it))
        {
            p->pack[i].count += it.count;
            return i;
        }
    if (p->pack_count >= PACK_SIZE)
        return -1;
    p->pack[p->pack_count] = it;
    return p->pack_count++;
}

/* Takes one of item i (the whole item if it doesn't stack) out of the pack. */
static Item take_one(Player *p, int i)
{
    Item one = p->pack[i];
    one.count = 1;
    if (--p->pack[i].count == 0)
    {
        memmove(&p->pack[i], &p->pack[i + 1], (size_t)(p->pack_count - i - 1) * sizeof p->pack[0]);
        p->pack_count--;
    }
    return one;
}

static void name(const Game *g, const Item *it, char out[NAME_SIZE])
{
    item_name(&g->lore, it, out, NAME_SIZE);
}

void inv_step_on(Game *g)
{
    Level *lv = game_level(g);
    int i = level_item_at(lv, g->player.pos);
    if (i < 0)
        return;
    Item *it = &lv->items[i].item;
    if (it->cls == ITEM_GOLD)
    {
        g->player.gold += it->count;
        msglog_add(&g->log, g->turn, "You pick up %d gold.", it->count);
        level_remove_item(lv, i);
        return;
    }
    char n[NAME_SIZE];
    name(g, it, n);
    msglog_add(&g->log, g->turn, "You see here %s.", n);
}

/* The pack letter of item i: a, b, ... */
static char letter(int i)
{
    return (char)('a' + i);
}

bool inv_pick_up(Game *g)
{
    Level *lv = game_level(g);
    int i = level_item_at(lv, g->player.pos);
    if (i < 0)
    {
        msglog_add(&g->log, g->turn, "There is nothing here to pick up.");
        return false;
    }
    int k = inv_add(&g->player, lv->items[i].item);
    if (k < 0)
    {
        msglog_add(&g->log, g->turn, "Your pack is full.");
        return false;
    }
    level_remove_item(lv, i);
    char n[NAME_SIZE];
    name(g, &g->player.pack[k], n);
    msglog_add(&g->log, g->turn, "%c - %s.", letter(k), n);
    return true;
}

static bool valid(Game *g, int i)
{
    return i >= 0 && i < g->player.pack_count;
}

bool inv_drop(Game *g, int i)
{
    if (!valid(g, i))
        return false;
    Level *lv = game_level(g);
    if (level_item_at(lv, g->player.pos) >= 0 ||
        map_get(g->map, g->player.pos.x, g->player.pos.y) != TILE_FLOOR)
    {
        msglog_add(&g->log, g->turn, "There is no room to drop it here.");
        return false;
    }
    Item it = take_one(&g->player, i);     /* one of a stack */
    level_add_item(lv, it, g->player.pos); /* unequips it */
    it.equipped = false;
    char n[NAME_SIZE];
    name(g, &it, n);
    msglog_add(&g->log, g->turn, "You drop %s.", n);
    return true;
}

/* ---- effects --------------------------------------------------------------- */

static void drink(Game *g, PotionKind kind)
{
    Player *p = &g->player;
    switch (kind)
    {
    case POTION_HEALING:
        p->hp += HEAL;
        if (p->hp > p->base.hp_max)
            p->hp = p->base.hp_max;
        p->blind = 0;
        msglog_add(&g->log, g->turn, "You feel better.");
        break;
    case POTION_EXTRA_HEALING:
        p->base.hp_max += EXTRA_HP_MAX;
        p->hp = p->base.hp_max;
        p->blind = 0;
        p->confused = 0;
        msglog_add(&g->log, g->turn, "You feel much better.");
        break;
    case POTION_POISON:
        msglog_add(&g->log, g->turn, "You feel very sick.");
        p->hp -= rng_range(&g->rng, POISON_MIN, POISON_MAX);
        if (p->hp <= 0)
            game_kill_player(g, DEATH_POISON);
        break;
    case POTION_SPEED:
        p->haste += HASTE_TICKS;
        msglog_add(&g->log, g->turn, "You feel yourself speed up.");
        break;
    case POTION_CONFUSION:
        p->confused += CONFUSION_TICKS;
        msglog_add(&g->log, g->turn, "You feel confused.");
        break;
    case POTION_BLINDNESS:
        p->blind += BLIND_TICKS;
        msglog_add(&g->log, g->turn, "Darkness falls over your eyes.");
        break;
    default:
        break;
    }
}

/* The equipped item of class cls, or NULL. */
static Item *equipped(Player *p, ItemClass cls)
{
    for (int i = 0; i < p->pack_count; i++)
        if (p->pack[i].cls == cls && p->pack[i].equipped)
            return &p->pack[i];
    return NULL;
}

static void enchant(Game *g, ItemClass cls, const char *glow, const char *nothing)
{
    Item *it = equipped(&g->player, cls);
    if (!it)
    {
        msglog_add(&g->log, g->turn, "%s", nothing);
        return;
    }
    it->enchant++;
    msglog_add(&g->log, g->turn, "Your %s glows %s.",
               cls == ITEM_WEAPON ? WEAPONS[it->kind].name : ARMOURS[it->kind].name, glow);
}

static void teleport(Game *g)
{
    for (int tries = 0; tries < 1000; tries++)
    {
        Vec2 to = vec2(rng_range(&g->rng, 0, g->map->w - 1), rng_range(&g->rng, 0, g->map->h - 1));
        if (map_get(g->map, to.x, to.y) == TILE_FLOOR && level_monster_at(game_level(g), to) < 0 &&
            !vec2_eq(to, g->player.pos))
        {
            g->player.pos = to;
            msglog_add(&g->log, g->turn, "You find yourself elsewhere.");
            game_look(g);
            return;
        }
    }
}

/* Floor, stairs and walls next to them become explored (remembered). */
static void map_level(Game *g)
{
    Map *m = g->map;
    for (int y = 0; y < m->h; y++)
        for (int x = 0; x < m->w; x++)
        {
            bool near_floor = false;
            for (int dy = -1; dy <= 1 && !near_floor; dy++)
                for (int dx = -1; dx <= 1 && !near_floor; dx++)
                    near_floor = map_walkable(m, x + dx, y + dy);
            if (near_floor)
                m->explored[y * m->w + x] = true;
        }
    msglog_add(&g->log, g->turn, "A map of this level forms in your mind.");
}

static void read(Game *g, ScrollKind kind)
{
    Player *p = &g->player;
    switch (kind)
    {
    case SCROLL_IDENTIFY:
        for (int i = 0; i < p->pack_count; i++)
            item_identify(&g->lore, &p->pack[i]);
        msglog_add(&g->log, g->turn, "Your pack glows: you know what everything in it is.");
        break;
    case SCROLL_MAGIC_MAPPING:
        map_level(g);
        break;
    case SCROLL_TELEPORT:
        teleport(g);
        break;
    case SCROLL_ENCHANT_WEAPON:
        enchant(g, ITEM_WEAPON, "blue", "Your hands twitch.");
        break;
    case SCROLL_ENCHANT_ARMOUR:
        enchant(g, ITEM_ARMOUR, "silver", "Your skin tingles.");
        break;
    case SCROLL_AGGRAVATE:
    {
        Level *lv = game_level(g);
        for (int i = 0; i < lv->monster_count; i++)
        {
            lv->monsters[i].awake = true;
            lv->monsters[i].track = AGGRAVATE_TRACK;
        }
        msglog_add(&g->log, g->turn, "You hear a high-pitched humming.");
        break;
    }
    default:
        break;
    }
}

/* Wield/wear item i, taking off whatever of its class is equipped. */
static void equip(Game *g, int i)
{
    Player *p = &g->player;
    Item *it = &p->pack[i];
    char n[NAME_SIZE];
    if (it->equipped)
    {
        it->equipped = false;
        name(g, it, n);
        msglog_add(&g->log, g->turn,
                   it->cls == ITEM_WEAPON ? "You put away %s." : "You take off %s.", n);
        return;
    }
    Item *old = equipped(p, it->cls);
    if (old)
        old->equipped = false;
    it->enchant_known = true; /* you feel how good it is */
    name(g, it, n);           /* before equipped: no "(wielded)" in the message */
    it->equipped = true;
    msglog_add(&g->log, g->turn,
               it->cls == ITEM_WEAPON ? "You are now wielding %s." : "You are now wearing %s.", n);
}

bool inv_use(Game *g, int i)
{
    if (!valid(g, i))
        return false;
    Player *p = &g->player;
    Item *it = &p->pack[i];
    char n[NAME_SIZE];

    switch (it->cls)
    {
    case ITEM_FOOD:
        take_one(p, i);
        p->nutrition += NUTRITION_RATION;
        if (p->nutrition > NUTRITION_MAX)
            p->nutrition = NUTRITION_MAX;
        msglog_add(&g->log, g->turn, "That food tasted good.");
        return true;
    case ITEM_POTION:
    case ITEM_SCROLL:
    {
        Item one = take_one(p, i);
        bool known = item_kind_known(&g->lore, &one);
        item_identify(&g->lore, &one); /* using it tells what it was */
        if (one.cls == ITEM_POTION)
            drink(g, (PotionKind)one.kind);
        else
            read(g, (ScrollKind)one.kind);
        if (!known)
        {
            name(g, &one, n);
            msglog_add(&g->log, g->turn, "(That was %s.)", n);
        }
        return true;
    }
    case ITEM_WEAPON:
    case ITEM_ARMOUR:
        equip(g, i);
        return true;
    default:
        return false;
    }
}
