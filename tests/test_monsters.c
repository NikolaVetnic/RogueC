#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "core/ui.h"

/* Depth 1 turned into one open floor, no monsters, player at (50, 20). */
static void open_game(Game *g)
{
    game_init(g, 17);
    clear_monsters(g);
    map_fill(g->map, TILE_FLOOR);
    g->player.pos = vec2(50, 20);
    game_step(g, ACTION_WAIT); /* look around */
}

static Monster *add(Game *g, MonsterKind kind, Vec2 pos, bool awake)
{
    Level *lv = game_level(g);
    Monster m = {.kind = kind, .pos = pos, .hp = MONSTERS[kind].stats.hp_max, .awake = awake};
    level_add_monster(lv, m);
    return &lv->monsters[lv->monster_count - 1];
}

static bool logged(const Game *g, const char *text)
{
    for (int i = 0; i < msglog_len(&g->log); i++)
        if (strstr(msglog_get(&g->log, i)->text, text))
            return true;
    return false;
}

static int chebyshev(Vec2 a, Vec2 b)
{
    int dx = abs(a.x - b.x), dy = abs(a.y - b.y);
    return dx > dy ? dx : dy;
}

static void hit_chance_is_clamped(void)
{
    Stats weak = {.to_hit = 0, .defense = 0}, strong = {.to_hit = 20, .defense = 20};
    CHECK(combat_hit_chance(&weak, &weak) == 60);
    CHECK(combat_hit_chance(&strong, &weak) == 95);
    CHECK(combat_hit_chance(&weak, &strong) == 5);
}

/* Energy: speed 150 acts once, then twice; a speed-50 player's move takes two ticks. */
static void speed_sets_how_often_one_acts(void)
{
    Game g;
    open_game(&g);
    Monster *bat = add(&g, MON_BAT, vec2(10, 5), true); /* far away, out of view */
    bat->energy = 0;
    game_tick(&g);
    CHECK(bat->energy == 50);
    game_tick(&g);
    CHECK(bat->energy == 0);

    int turn = g.turn;
    game_step(&g, ACTION_WAIT);
    CHECK(g.turn == turn + 1);
    g.player.base.speed = 50;
    turn = g.turn;
    game_step(&g, ACTION_WAIT);
    CHECK(g.turn == turn + 2);
    game_free(&g);
}

static void asleep_out_of_view_stays_put(void)
{
    Game g;
    open_game(&g);
    Monster *m = add(&g, MON_GOBLIN, vec2(90, 20), false);
    for (int i = 0; i < 50; i++)
        game_step(&g, ACTION_WAIT);
    CHECK(vec2_eq(m->pos, vec2(90, 20)) && !m->awake);
    game_free(&g);
}

/* The exit check, part one: a monster chases and fights. */
static void awake_monster_chases_and_attacks(void)
{
    Game g;
    open_game(&g);
    Monster *m = add(&g, MON_GOBLIN, vec2(58, 24), true);
    for (int i = 0; i < 12; i++)
        game_step(&g, ACTION_WAIT);
    CHECK(chebyshev(m->pos, g.player.pos) == 1);
    CHECK(logged(&g, "You see a goblin."));
    CHECK(logged(&g, "The goblin hits you.") || logged(&g, "The goblin misses you."));
    game_free(&g);
}

/* Part two: it dies... */
static void player_kills_a_monster(void)
{
    Game g;
    open_game(&g);
    add(&g, MON_RAT, vec2(51, 20), false);
    for (int i = 0; i < 60 && game_level(&g)->monster_count > 0; i++)
        game_step(&g, ACTION_MOVE_E); /* attacks: the rat is east */
    CHECK(game_level(&g)->monster_count == 0);
    CHECK(logged(&g, "You kill the rat."));
    CHECK(vec2_eq(g.player.pos, vec2(50, 20)) || vec2_eq(g.player.pos, vec2(51, 20)));
    game_free(&g);
}

/* ...or kills: the death screen, then a new game or quit. */
static void monster_kills_the_player(void)
{
    enum
    {
        COLS = 80,
        ROWS = 24,
    };
    static Cell cells[COLS * ROWS];
    Game g;
    Ui ui;
    open_game(&g);
    ui_init(&ui, NULL);
    g.player.hp = 1;
    add(&g, MON_TROLL, vec2(51, 20), true);
    for (int i = 0; i < 100 && !g.dead; i++)
        ui_key(&ui, &g, '.');
    CHECK(g.dead && g.player.hp == 0 && g.killer == MON_TROLL);
    CHECK(logged(&g, "You die."));
    CHECK(ui.screen == SCREEN_END);
    CHECK(!game_step(&g, ACTION_WAIT));

    ui_render(&ui, &g, cells, COLS, ROWS);
    char row[COLS + 1] = {0};
    for (int x = 0; x < COLS; x++)
        row[x] = (char)cells[3 * COLS + x].glyph;
    CHECK(strstr(row, "Killed by a troll on depth 1") != NULL);

    ui_key(&ui, &g, 'd'); /* other keys do nothing */
    CHECK(!ui.new_game && g.running);
    ui_key(&ui, &g, '\n');
    CHECK(ui.new_game);
    ui_key(&ui, &g, 'q');
    CHECK(!g.running);
    game_free(&g);
}

/* Many hunters: never two in a cell, never on the player or in rock. */
static void monsters_keep_their_own_cells(void)
{
    Game g;
    open_game(&g);
    for (int i = 0; i < 12; i++)
        add(&g, MON_KOBOLD, vec2(40 + i, 10 + i % 3), true);
    for (int i = 0; i < 12; i++) /* a box of rock with a gap, to crowd them */
        map_set(g.map, 44 + i, 15, i == 6 ? TILE_FLOOR : TILE_WALL);
    g.player.hp = 1000;
    int clashes = 0;
    for (int t = 0; t < 150; t++)
    {
        game_step(&g, ACTION_WAIT);
        const Level *lv = game_level(&g);
        for (int i = 0; i < lv->monster_count; i++)
        {
            Vec2 p = lv->monsters[i].pos;
            clashes += vec2_eq(p, g.player.pos) || !map_walkable(g.map, p.x, p.y);
            for (int j = i + 1; j < lv->monster_count; j++)
                clashes += vec2_eq(p, lv->monsters[j].pos);
        }
    }
    CHECK(clashes == 0);
    game_free(&g);
}

static void player_heals_and_rests(void)
{
    Game g;
    open_game(&g);
    g.player.hp = 10;
    int turn = g.turn;
    for (int i = 0; i < 30; i++)
        game_step(&g, ACTION_WAIT);
    CHECK(g.player.hp == 13); /* 1 HP every 10 turns */
    CHECK(g.turn == turn + 30);

    CHECK(game_step(&g, ACTION_REST));
    CHECK(g.player.hp == g.player.base.hp_max);
    CHECK(logged(&g, "You rest and feel better."));

    g.player.hp = 5;
    add(&g, MON_RAT, vec2(53, 20), false);
    game_step(&g, ACTION_WAIT); /* sees it */
    turn = g.turn;
    CHECK(!game_step(&g, ACTION_REST));
    CHECK(g.turn == turn && logged(&g, "You can't rest with a monster in view."));
    game_free(&g);
}

/* Real time: one move per tick; a key pressed early waits in the ui. */
static void real_time_runs_on_the_clock(void)
{
    Game g;
    Ui ui;
    open_game(&g);
    ui_init(&ui, NULL);
    ui_key(&ui, &g, 'T');
    CHECK(g.realtime);
    int turn = g.turn;

    ui_key(&ui, &g, 'd');
    CHECK(vec2_eq(g.player.pos, vec2(51, 20)) && g.turn == turn); /* moved, no tick */
    ui_key(&ui, &g, 'd');                                         /* not ready: kept */
    CHECK(vec2_eq(g.player.pos, vec2(51, 20)) && ui.pending == ACTION_MOVE_E);

    ui_update(&ui, &g, UI_TICK_MS - 1);
    CHECK(g.turn == turn);
    ui_update(&ui, &g, 1); /* a tick: ready, the kept move happens */
    CHECK(g.turn == turn + 1 && vec2_eq(g.player.pos, vec2(52, 20)));
    CHECK(ui.pending == ACTION_NONE);

    ui_key(&ui, &g, '?'); /* help pauses the clock */
    ui_update(&ui, &g, 10 * UI_TICK_MS);
    CHECK(g.turn == turn + 1);
    ui_key(&ui, &g, 27);
    ui_update(&ui, &g, 100 * UI_TICK_MS); /* a long stall catches up at most 4 ticks */
    CHECK(g.turn == turn + 5);

    ui_key(&ui, &g, 'T');
    CHECK(!g.realtime);
    game_free(&g);
}

static void visible_monsters_are_drawn(void)
{
    enum
    {
        COLS = 60,
        ROWS = 20,
    };
    static Cell cells[COLS * ROWS];
    Game g;
    open_game(&g);
    add(&g, MON_ORC, vec2(53, 20), false);
    add(&g, MON_OGRE, vec2(80, 20), false); /* out of view */
    game_step(&g, ACTION_WAIT);
    render_game(&g, cells, COLS, ROWS);
    int orcs = 0, ogres = 0;
    for (int i = 0; i < COLS * ROWS; i++)
    {
        orcs += cells[i].glyph == 'o' && cells[i].style == STYLE_MON_HUMANOID;
        ogres += cells[i].glyph == 'O';
    }
    CHECK(orcs == 1 && ogres == 0);
    game_free(&g);
}

void run_monster_tests(void)
{
    RUN(hit_chance_is_clamped);
    RUN(speed_sets_how_often_one_acts);
    RUN(asleep_out_of_view_stays_put);
    RUN(awake_monster_chases_and_attacks);
    RUN(player_kills_a_monster);
    RUN(monster_kills_the_player);
    RUN(monsters_keep_their_own_cells);
    RUN(player_heals_and_rests);
    RUN(real_time_runs_on_the_clock);
    RUN(visible_monsters_are_drawn);
}
