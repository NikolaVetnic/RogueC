#include "test.h"

#include <string.h>

#include "core/inventory.h"
#include "core/ui.h"

/* Depth 1 as open floor, no monsters, player at (50, 20) with the starting kit. */
static void open_game(Game *g)
{
    game_init(g, 23);
    clear_monsters(g);
    game_level(g)->item_count = 0;
    map_fill(g->map, TILE_FLOOR);
    g->player.pos = vec2(50, 20);
    game_step(g, ACTION_WAIT);
}

static void put_here(Game *g, Item it)
{
    level_add_item(game_level(g), it, g->player.pos);
}

static bool logged(const Game *g, const char *text)
{
    for (int i = 0; i < msglog_len(&g->log); i++)
        if (strstr(msglog_get(&g->log, i)->text, text))
            return true;
    return false;
}

static const char *pack_name(const Game *g, int i)
{
    static char n[80];
    item_name(&g->lore, &g->player.pack[i], n, sizeof n);
    return n;
}

static void looks_differ_and_follow_the_seed(void)
{
    Game a, b;
    game_init(&a, 5);
    game_init(&b, 5);
    CHECK(memcmp(&a.lore, &b.lore, sizeof a.lore) == 0);
    for (int i = 0; i < POTION_COUNT; i++)
        for (int j = i + 1; j < POTION_COUNT; j++)
            CHECK(a.lore.potion_look[i] != a.lore.potion_look[j]);
    for (int k = 0; k < SCROLL_COUNT; k++)
        CHECK(strlen(a.lore.scroll_title[k]) >= 2);
    game_free(&a);
    game_free(&b);
}

static void names_read_well(void)
{
    ItemLore lore = {0};
    char n[80];
    Item food = {.cls = ITEM_FOOD, .count = 1}, rations = {.cls = ITEM_FOOD, .count = 2};
    item_name(&lore, &food, n, sizeof n);
    CHECK(strcmp(n, "a food ration") == 0);
    item_name(&lore, &rations, n, sizeof n);
    CHECK(strcmp(n, "2 food rations") == 0);

    Item potion = {.cls = ITEM_POTION, .kind = POTION_HEALING, .count = 1};
    lore.potion_look[POTION_HEALING] = 2; /* green */
    item_name(&lore, &potion, n, sizeof n);
    CHECK(strcmp(n, "a green potion") == 0);
    lore.potion_known[POTION_HEALING] = true;
    item_name(&lore, &potion, n, sizeof n);
    CHECK(strcmp(n, "a potion of healing") == 0);

    Item mace = {.cls = ITEM_WEAPON, .kind = WEAPON_MACE, .count = 1, .enchant = 1};
    item_name(&lore, &mace, n, sizeof n);
    CHECK(strcmp(n, "a mace") == 0); /* bonus unknown */
    mace.enchant_known = mace.equipped = true;
    item_name(&lore, &mace, n, sizeof n);
    CHECK(strcmp(n, "a +1 mace (wielded)") == 0);
}

static void starting_kit_sets_stats(void)
{
    Game g;
    open_game(&g);
    CHECK(g.player.pack_count == 3);
    CHECK(strcmp(pack_name(&g, 0), "a +0 short sword (wielded)") == 0);
    CHECK(strcmp(pack_name(&g, 1), "+0 leather armour (worn)") == 0);
    Stats s = game_player_stats(&g);
    CHECK(s.to_hit == 2 && s.defense == 2 && s.dmg_min == 2 && s.dmg_max == 6);
    game_free(&g);
}

/* The exit check: pick up... */
static void pick_up_stacks_and_takes_a_turn(void)
{
    Game g;
    open_game(&g);
    int turn = g.turn;
    CHECK(!game_step(&g, ACTION_PICK_UP) && g.turn == turn);
    CHECK(logged(&g, "There is nothing here to pick up."));

    Item potion = {.cls = ITEM_POTION, .kind = POTION_SPEED, .count = 1};
    put_here(&g, potion);
    CHECK(game_step(&g, ACTION_PICK_UP) && g.turn == turn + 1);
    CHECK(g.player.pack_count == 4 && game_level(&g)->item_count == 0);
    CHECK(logged(&g, "d - a "));
    put_here(&g, potion);
    game_step(&g, ACTION_PICK_UP);
    CHECK(g.player.pack_count == 4 && g.player.pack[3].count == 2);
    game_free(&g);
}

static void gold_is_picked_up_by_walking(void)
{
    Game g;
    open_game(&g);
    level_add_item(game_level(&g), (Item){.cls = ITEM_GOLD, .count = 37}, vec2(51, 20));
    game_step(&g, ACTION_MOVE_E);
    CHECK(g.player.gold == 37 && game_level(&g)->item_count == 0);
    CHECK(logged(&g, "You pick up 37 gold."));
    game_free(&g);
}

/* ...use and identify... */
static void using_identifies(void)
{
    Game g;
    open_game(&g);
    g.player.hp = 3;
    inv_add(&g.player, (Item){.cls = ITEM_POTION, .kind = POTION_HEALING, .count = 2});
    CHECK(!g.lore.potion_known[POTION_HEALING]);
    CHECK(game_step_item(&g, ACTION_USE, 3));
    CHECK(g.lore.potion_known[POTION_HEALING] && g.player.hp == 18);
    CHECK(logged(&g, "(That was a potion of healing.)"));
    CHECK(g.player.pack[3].count == 1);

    inv_add(&g.player, (Item){.cls = ITEM_POTION, .kind = POTION_POISON, .count = 1});
    inv_add(&g.player, (Item){.cls = ITEM_SCROLL, .kind = SCROLL_IDENTIFY, .count = 1});
    inv_add(&g.player, (Item){.cls = ITEM_WEAPON, .kind = WEAPON_MACE, .count = 1, .enchant = 2});
    game_step_item(&g, ACTION_USE, 5); /* identify */
    CHECK(g.lore.potion_known[POTION_POISON] && g.lore.scroll_known[SCROLL_IDENTIFY]);
    CHECK(g.player.pack[5].enchant_known); /* the mace, moved up after the scroll was used */
    CHECK(g.player.pack_count == 6);
    game_free(&g);
}

/* ...and equip. */
static void wielding_and_wearing_change_stats(void)
{
    Game g;
    open_game(&g);
    inv_add(&g.player, (Item){.cls = ITEM_WEAPON, .kind = WEAPON_MACE, .count = 1, .enchant = 1});
    inv_add(&g.player, (Item){.cls = ITEM_ARMOUR, .kind = ARMOUR_CHAIN, .count = 1});
    game_step_item(&g, ACTION_USE, 3);
    CHECK(g.player.pack[3].equipped && !g.player.pack[0].equipped);
    CHECK(logged(&g, "You are now wielding a +1 mace."));
    Stats s = game_player_stats(&g);
    CHECK(s.dmg_min == 3 && s.dmg_max == 9 && s.to_hit == 2);

    game_step_item(&g, ACTION_USE, 4);
    CHECK(game_player_stats(&g).defense == 5 && !g.player.pack[1].equipped);

    game_step_item(&g, ACTION_USE, 3); /* put the mace away: bare hands */
    s = game_player_stats(&g);
    CHECK(!g.player.pack[3].equipped && s.dmg_min == 1 && s.dmg_max == 2);
    game_free(&g);
}

static void dropping_puts_it_on_the_floor(void)
{
    Game g;
    open_game(&g);
    CHECK(game_step_item(&g, ACTION_DROP, 0)); /* the wielded sword */
    CHECK(g.player.pack_count == 2 && game_level(&g)->item_count == 1);
    CHECK(!game_level(&g)->items[0].item.equipped);
    CHECK(game_player_stats(&g).dmg_max == 2);
    CHECK(!game_step_item(&g, ACTION_DROP, 0)); /* something lies here already */
    CHECK(logged(&g, "There is no room to drop it here."));
    CHECK(!game_step_item(&g, ACTION_DROP, 9)); /* no such item */
    game_free(&g);
}

static void hunger_warns_weakens_and_kills(void)
{
    Game g;
    open_game(&g);
    g.player.nutrition = HUNGRY_BELOW;
    game_step(&g, ACTION_WAIT);
    CHECK(game_hunger(&g) == HUNGER_HUNGRY && logged(&g, "You are getting hungry."));

    g.player.nutrition = 1;
    g.player.hp = 3;
    for (int i = 0; i < 40 && !g.dead; i++)
        game_step(&g, ACTION_WAIT);
    CHECK(g.dead && g.death == DEATH_STARVATION);
    game_free(&g);

    open_game(&g);
    g.player.nutrition = 50;
    game_step_item(&g, ACTION_USE, 2);                      /* the ration */
    CHECK(g.player.nutrition == 50 + NUTRITION_RATION - 1); /* eaten, then one tick */
    CHECK(g.player.pack_count == 2);
    game_free(&g);
}

static void potions_have_effects(void)
{
    Game g;
    open_game(&g);
    g.player.haste = 4;
    CHECK(game_player_stats(&g).speed == 200);
    int turn = g.turn;
    game_step(&g, ACTION_WAIT);
    game_step(&g, ACTION_WAIT);
    CHECK(g.turn == turn + 1); /* two actions, one tick */
    for (int i = 0; i < 10; i++)
        game_step(&g, ACTION_WAIT);
    CHECK(g.player.haste == 0 && logged(&g, "You feel yourself slow down."));

    g.player.blind = 3;
    game_step(&g, ACTION_WAIT);
    CHECK(map_is_visible(g.map, 51, 20) && !map_is_visible(g.map, 53, 20));
    for (int i = 0; i < 3; i++)
        game_step(&g, ACTION_WAIT);
    CHECK(map_is_visible(g.map, 53, 20) && logged(&g, "You can see again."));

    g.player.confused = 100;
    int off = 0;
    for (int i = 0; i < 40; i++)
    {
        Vec2 before = g.player.pos;
        game_step(&g, ACTION_MOVE_E);
        off += !vec2_eq(g.player.pos, vec2_add(before, vec2(1, 0)));
    }
    CHECK(off > 5 && off < 35); /* about half go astray */

    g.player.hp = 2;
    inv_add(&g.player, (Item){.cls = ITEM_POTION, .kind = POTION_POISON, .count = 1});
    game_step_item(&g, ACTION_USE, g.player.pack_count - 1);
    CHECK(g.dead && g.death == DEATH_POISON);
    game_free(&g);
}

static void scrolls_have_effects(void)
{
    Game g;
    open_game(&g);
    game_level(&g)->monster_count = 0;
    level_add_monster(game_level(&g), (Monster){.kind = MON_RAT, .pos = vec2(90, 30), .hp = 3});

    inv_add(&g.player, (Item){.cls = ITEM_SCROLL, .kind = SCROLL_ENCHANT_WEAPON, .count = 1});
    game_step_item(&g, ACTION_USE, 3);
    CHECK(g.player.pack[0].enchant == 1 && logged(&g, "Your short sword glows blue."));

    inv_add(&g.player, (Item){.cls = ITEM_SCROLL, .kind = SCROLL_AGGRAVATE, .count = 1});
    game_step_item(&g, ACTION_USE, 3);
    CHECK(game_level(&g)->monsters[0].awake);

    inv_add(&g.player, (Item){.cls = ITEM_SCROLL, .kind = SCROLL_MAGIC_MAPPING, .count = 1});
    game_step_item(&g, ACTION_USE, 3);
    int explored = 0;
    for (int i = 0; i < g.map->w * g.map->h; i++)
        explored += g.map->explored[i];
    CHECK(explored == g.map->w * g.map->h); /* all floor here */

    Vec2 before = g.player.pos;
    inv_add(&g.player, (Item){.cls = ITEM_SCROLL, .kind = SCROLL_TELEPORT, .count = 1});
    game_step_item(&g, ACTION_USE, 3);
    CHECK(!vec2_eq(g.player.pos, before) && logged(&g, "You find yourself elsewhere."));
    game_free(&g);
}

/* Inventory screen: i + letter uses, D + letter drops, other keys go back. */
static void pack_screen_uses_and_drops(void)
{
    enum
    {
        COLS = 90,
        ROWS = 30,
    };
    static Cell cells[COLS * ROWS];
    Game g;
    Ui ui;
    open_game(&g);
    ui_init(&ui, NULL);

    ui_key(&ui, &g, 'i');
    CHECK(ui.screen == SCREEN_PACK);
    ui_render(&ui, &g, cells, COLS, ROWS);
    char row[COLS + 1] = {0};
    for (int x = 0; x < COLS; x++)
        row[x] = (char)cells[3 * COLS + x].glyph;
    CHECK(strstr(row, "a - a +0 short sword (wielded)") != NULL);

    ui_key(&ui, &g, 'c'); /* eat the ration */
    CHECK(ui.screen == SCREEN_MAP && g.player.pack_count == 2);
    ui_key(&ui, &g, 'D');
    ui_key(&ui, &g, 'b'); /* drop the armour */
    CHECK(g.player.pack_count == 1 && game_level(&g)->item_count == 1);
    ui_key(&ui, &g, 'i');
    ui_key(&ui, &g, 27);
    CHECK(ui.screen == SCREEN_MAP && g.player.pack_count == 1);
    ui_key(&ui, &g, 'g'); /* pick the armour up again */
    CHECK(g.player.pack_count == 2);
    game_free(&g);
}

static void levels_have_gold_and_items(void)
{
    for (uint64_t seed = 0; seed < 10; seed++)
        for (int depth = 1; depth <= 10; depth++)
        {
            Level lv;
            level_generate(&lv, seed, depth, 10, 100, 42);
            int gold = 0, other = 0, goal = 0;
            for (int i = 0; i < lv.item_count; i++)
            {
                const FloorItem *f = &lv.items[i];
                gold += f->item.cls == ITEM_GOLD;
                other += f->item.cls != ITEM_GOLD && f->item.cls != ITEM_GOAL;
                goal += f->item.cls == ITEM_GOAL;
                CHECK(map_get(&lv.map, f->pos.x, f->pos.y) == TILE_FLOOR);
                CHECK(level_item_at(&lv, f->pos) == i);
                if (f->item.cls == ITEM_WEAPON)
                    CHECK(WEAPONS[f->item.kind].min_depth <= depth);
            }
            CHECK(gold >= 2 && gold <= 4);
            CHECK(other == 3 + depth / 3);
            CHECK(goal == (depth == 10)); /* the Sunstone, on the bottom level only */
            level_free(&lv);
        }
}

void run_item_tests(void)
{
    RUN(looks_differ_and_follow_the_seed);
    RUN(names_read_well);
    RUN(starting_kit_sets_stats);
    RUN(pick_up_stacks_and_takes_a_turn);
    RUN(gold_is_picked_up_by_walking);
    RUN(using_identifies);
    RUN(wielding_and_wearing_change_stats);
    RUN(dropping_puts_it_on_the_floor);
    RUN(hunger_warns_weakens_and_kills);
    RUN(potions_have_effects);
    RUN(scrolls_have_effects);
    RUN(pack_screen_uses_and_drops);
    RUN(levels_have_gold_and_items);
}
