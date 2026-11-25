#include "test.h"

#include <string.h>

#include "core/inventory.h"
#include "core/score.h"
#include "core/ui.h"

static void open_game(Game *g)
{
    game_init(g, 31);
    clear_monsters(g);
    map_fill(g->map, TILE_FLOOR);
    g->player.pos = vec2(50, 20);
    game_step(g, ACTION_WAIT);
}

static bool logged(const Game *g, const char *text)
{
    for (int i = 0; i < msglog_len(&g->log); i++)
        if (strstr(msglog_get(&g->log, i)->text, text))
            return true;
    return false;
}

/* Kills a monster of `kind` placed east of the player. */
static void kill(Game *g, MonsterKind kind)
{
    Level *lv = game_level(g);
    level_add_monster(lv,
                      (Monster){.kind = kind, .pos = vec2_add(g->player.pos, vec2(1, 0)), .hp = 1});
    for (int i = 0; i < 200 && level_monster_at(lv, vec2_add(g->player.pos, vec2(1, 0))) >= 0; i++)
        game_step(g, ACTION_MOVE_E);
}

static void kills_give_experience_and_levels(void)
{
    Game g;
    open_game(&g);
    CHECK(g.player.level == 1 && g.player.xp == 0);
    for (int l = 2; l <= MAX_LEVEL; l++)
        CHECK(game_xp_needed(l) > game_xp_needed(l - 1));

    g.player.xp = 9;
    Stats before = game_player_stats(&g);
    kill(&g, MON_RAT); /* 1 xp: 10, level 2 */
    CHECK(g.player.xp == 10 && g.player.level == 2);
    CHECK(g.player.base.hp_max == 20 + LEVEL_HP && logged(&g, "Welcome to level 2."));
    Stats after = game_player_stats(&g);
    CHECK(after.to_hit == before.to_hit + 1 && after.dmg_max == before.dmg_max);

    kill(&g, MON_ORC);    /* 12 xp: 22 */
    kill(&g, MON_RAT);    /* 23 */
    kill(&g, MON_GOBLIN); /* 28: level 3 */
    CHECK(g.player.level == 3 && game_player_stats(&g).dmg_max == before.dmg_max + 1);

    g.player.xp = 100000; /* many levels at once, capped */
    kill(&g, MON_RAT);
    CHECK(g.player.level == MAX_LEVEL);
    game_free(&g);
}

static void healing_speeds_up_with_level(void)
{
    Game g;
    open_game(&g);
    g.player.level = 8; /* every 3 turns */
    g.player.base.hp_max = 100;
    g.player.hp = 10;
    for (int i = 0; i < 30; i++)
        game_step(&g, ACTION_WAIT);
    CHECK(g.player.hp == 20);
    game_free(&g);
}

/* The exit check: a full game can be won... */
static void carrying_the_goal_out_wins(void)
{
    Game g;
    Ui ui;
    ScoreTable table = {0};
    open_game(&g);
    ui_init(&ui, &table, NULL);

    Vec2 exit = g.levels[0].stairs_up;
    map_set(g.map, exit.x, exit.y, TILE_STAIRS_UP);
    g.player.pos = exit;
    CHECK(!game_step(&g, ACTION_ASCEND) && !g.won);

    /* The goal lies on the bottom level; carry it up. */
    const Level *bottom = &g.levels[DUNGEON_DEPTH - 1];
    int found = 0;
    for (int i = 0; i < bottom->item_count; i++)
        found += bottom->items[i].item.cls == ITEM_GOAL;
    CHECK(found == 1);
    level_add_item(game_level(&g), (Item){.cls = ITEM_GOAL, .count = 1}, g.player.pos);
    CHECK(game_step(&g, ACTION_PICK_UP) && game_has_goal(&g));
    CHECK(g.wander_odds == 0 ||
          g.wander_odds == GOAL_WANDER_ODDS); /* 0: switched off by the test helper */
    CHECK(logged(&g, "The dungeon stirs."));

    ui_key(&ui, &g, '<');
    CHECK(g.won && game_over(&g) && !g.dead);
    CHECK(ui.screen == SCREEN_END && ui.scores_changed && ui.score_rank == 0);
    CHECK(table.count == 1 && table.entries[0].won);
    CHECK(table.entries[0].points >= WIN_BONUS);
    CHECK(strcmp(table.entries[0].cause, "escaped with the Sunstone") == 0);
    CHECK(!game_step(&g, ACTION_WAIT)); /* over */

    enum
    {
        COLS = 90,
        ROWS = 30,
    };
    static Cell cells[COLS * ROWS];
    ui_render(&ui, &g, cells, COLS, ROWS);
    char row[COLS + 1] = {0};
    for (int x = 0; x < COLS; x++)
        row[x] = (char)cells[1 * COLS + x].glyph;
    CHECK(strstr(row, "You won!") != NULL);
    game_free(&g);
}

/* ...or lost, and the score goes into the table once. */
static void death_scores_once(void)
{
    Game g;
    Ui ui;
    ScoreTable table = {0};
    open_game(&g);
    ui_init(&ui, &table, NULL);
    g.player.gold = 123;
    g.player.xp = 7;
    g.player.hp = 1;
    level_add_monster(game_level(&g),
                      (Monster){.kind = MON_TROLL, .pos = vec2(51, 20), .hp = 30, .awake = true});
    for (int i = 0; i < 100 && !g.dead; i++)
        ui_key(&ui, &g, '.');
    CHECK(g.dead && ui.screen == SCREEN_END && table.count == 1);
    CHECK(table.entries[0].points == 123 + 70 + 100 * 1);
    CHECK(strcmp(table.entries[0].cause, "killed by a troll on depth 1") == 0);
    ui_key(&ui, &g, 'x'); /* staying on the end screen adds nothing more */
    ui_update(&ui, &g, 1000);
    CHECK(table.count == 1);
    game_free(&g);
}

static Score score(int points, const char *cause)
{
    Score s = {.points = points, .deepest = 3, .level = 2, .turns = 100};
    snprintf(s.cause, sizeof s.cause, "%s", cause);
    return s;
}

static void table_keeps_the_best_ten(void)
{
    ScoreTable t = {0};
    CHECK(scores_add(&t, score(50, "a")) == 0);
    CHECK(scores_add(&t, score(80, "b")) == 0);
    CHECK(scores_add(&t, score(60, "c")) == 1);
    CHECK(t.count == 3 && t.entries[0].points == 80 && t.entries[2].points == 50);
    for (int i = 0; i < 20; i++)
        scores_add(&t, score(100 + i, "x"));
    CHECK(t.count == SCORES_MAX && t.entries[0].points == 119 && t.entries[9].points == 110);
    CHECK(scores_add(&t, score(5, "too low")) == -1);
}

static void table_survives_text(void)
{
    ScoreTable t = {0}, back;
    scores_add(&t, score(900, "killed by an orc on depth 5"));
    Score won = score(3000, "escaped with the Sunstone");
    won.won = true;
    scores_add(&t, won);
    scores_add(&t, score(10, "it's \"quoted\" | piped")); /* cleaned for storage */

    char text[2048];
    CHECK(scores_format(&t, text, sizeof text));
    scores_parse(&back, text);
    CHECK(back.count == 3);
    CHECK(back.entries[0].won && back.entries[0].points == 3000);
    CHECK(strcmp(back.entries[1].cause, "killed by an orc on depth 5") == 0);
    CHECK(strchr(back.entries[2].cause, '"') == NULL && strchr(back.entries[2].cause, '|') == NULL);
    CHECK(!scores_format(&t, text, 20)); /* too small */

    scores_parse(&back, "garbage\n12 3\n\n500 4 3 900 0 starved on depth 4\n");
    CHECK(back.count == 1 && back.entries[0].points == 500);
    scores_parse(&back, "");
    CHECK(back.count == 0);
}

void run_progression_tests(void)
{
    RUN(kills_give_experience_and_levels);
    RUN(healing_speeds_up_with_level);
    RUN(carrying_the_goal_out_wins);
    RUN(death_scores_once);
    RUN(table_keeps_the_best_ten);
    RUN(table_survives_text);
}
