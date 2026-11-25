#include "test.h"

#include <string.h>

#include "core/ui.h"

enum
{
    COLS = 80,
    ROWS = 30,
};
static Cell cells[COLS * ROWS];

/* Whether row y starts with `text` at column x. */
static bool row_has(int x, int y, const char *text)
{
    for (int i = 0; text[i] != '\0'; i++)
        if (cells[y * COLS + x + i].glyph != (unsigned char)text[i])
            return false;
    return true;
}

static bool screen_has(const char *text)
{
    int n = (int)strlen(text);
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x + n <= COLS; x++)
            if (row_has(x, y, text))
                return true;
    return false;
}

/* A game in an open area, so moves always succeed. */
static void open_game(Game *g)
{
    game_init(g, 11);
    clear_monsters(g);
    map_fill(g->map, TILE_FLOOR);
    g->player.pos = vec2(50, 20);
}

static void keys_move_on_the_map_screen(void)
{
    Game g;
    Ui ui;
    open_game(&g);
    ui_init(&ui, NULL, NULL);
    ui.can_exit = true;

    ui_key(&ui, &g, 'd');
    CHECK(vec2_eq(g.player.pos, vec2(51, 20)));
    ui_key(&ui, &g, UI_KEY_UP);
    CHECK(vec2_eq(g.player.pos, vec2(51, 19)));
    ui_key(&ui, &g, UI_KEY_PAGE_DOWN); /* south-east */
    CHECK(vec2_eq(g.player.pos, vec2(52, 20)));
    ui_key(&ui, &g, UI_KEY_CENTER);
    CHECK(g.turn == 4);
    ui_key(&ui, &g, 'q');
    CHECK(!g.running && ui.save_wanted && !ui.save_delete);
    game_free(&g);
}

static void help_opens_and_any_key_closes(void)
{
    Game g;
    Ui ui;
    open_game(&g);
    ui_init(&ui, NULL, NULL);

    ui_key(&ui, &g, '?');
    CHECK(ui.screen == SCREEN_HELP);
    ui_render(&ui, &g, cells, COLS, ROWS);
    CHECK(screen_has("Help") && screen_has("Message history") &&
          screen_has("Save (closing the tab"));
    CHECK(!screen_has("Depth")); /* not the map screen (no status line) */

    ui_key(&ui, &g, 'd'); /* closes help, does not move */
    CHECK(ui.screen == SCREEN_MAP);
    CHECK(vec2_eq(g.player.pos, vec2(50, 20)) && g.turn == 0);

    ui_key(&ui, &g, '?');
    ui_key(&ui, &g, 'q'); /* closes help, does not quit */
    CHECK(ui.screen == SCREEN_MAP && g.running);
    game_free(&g);
}

static void history_shows_messages_and_scrolls(void)
{
    Game g;
    Ui ui;
    open_game(&g);
    ui_init(&ui, NULL, NULL);
    for (int i = 0; i < 60; i++)
        msglog_add(&g.log, i, "Event %d", i);

    ui_key(&ui, &g, 'm');
    CHECK(ui.screen == SCREEN_LOG);
    ui_render(&ui, &g, cells, COLS, ROWS);
    CHECK(screen_has("Message history"));
    CHECK(row_has(2, ROWS - 1, "   59  Event 59")); /* newest at the bottom */
    CHECK(!screen_has("Event 20 "));

    ui_key(&ui, &g, UI_KEY_UP);
    ui_render(&ui, &g, cells, COLS, ROWS);
    CHECK(row_has(2, ROWS - 1, "   58  Event 58"));

    ui_key(&ui, &g, UI_KEY_HOME); /* oldest: the welcome message on the top line */
    ui_render(&ui, &g, cells, COLS, ROWS);
    CHECK(row_has(2, 3, "    0  You enter the dungeon."));
    CHECK(ui.scroll == 61 - (ROWS - 3)); /* clamped to a full page */

    ui_key(&ui, &g, UI_KEY_END);
    CHECK(ui.scroll == 0);
    ui_key(&ui, &g, UI_KEY_DOWN);
    CHECK(ui.scroll == 0);

    msglog_clear(&g.log); /* a short history starts at the top */
    msglog_add(&g.log, 0, "Only one");
    ui_render(&ui, &g, cells, COLS, ROWS);
    CHECK(row_has(2, 3, "    0  Only one"));

    ui_key(&ui, &g, 27); /* Esc */
    CHECK(ui.screen == SCREEN_MAP && g.turn == 0);
    game_free(&g);
}

static void screens_survive_tiny_sizes(void)
{
    Game g;
    Ui ui;
    Settings settings = settings_default();
    open_game(&g);
    for (Screen s = SCREEN_MAP; s <= SCREEN_OPTIONS; s++)
        for (int c = 0; c <= 3; c++)
            for (int r = 0; r <= 3; r++)
            {
                ui = (Ui){.screen = s, .scroll = 5, .settings = &settings, .colour_option = true};
                ui_render(&ui, &g, cells, c, r);
            }
    CHECK(true); /* no crash, no sanitizer report */
    game_free(&g);
}

/* Rock with a corridor from (10,10) east to (20,10), then south into a room
 * (15-25, 19-24) through a gap in its top wall at (20,18). */
static void corridor_game(Game *g)
{
    game_init(g, 11);
    clear_monsters(g);
    game_level(g)->item_count = 0;
    map_fill(g->map, TILE_WALL);
    for (int x = 10; x <= 20; x++)
        map_set(g->map, x, 10, TILE_FLOOR);
    for (int y = 11; y <= 18; y++)
        map_set(g->map, 20, y, TILE_FLOOR);
    for (int y = 19; y <= 24; y++)
        for (int x = 15; x <= 25; x++)
            map_set(g->map, x, y, TILE_FLOOR);
    g->player.pos = vec2(10, 10);
    game_look(g);
}

/* Shift runs: around corners, stopping where the corridor opens into the
 * room, at forks, beside doorways and on items. */
static void runs_follow_corridors_and_stop(void)
{
    Game g;
    Ui ui;
    corridor_game(&g);
    ui_init(&ui, NULL, NULL);
    ui_key(&ui, &g, 'L'); /* run east */
    CHECK(vec2_eq(g.player.pos, vec2(20, 19)) && g.turn == 19);
    CHECK(!game_run_active(&g));

    g.player.pos = vec2(16, 19); /* along the room's top wall: stops beside the gap */
    ui_key(&ui, &g, UI_KEY_SHIFT | UI_KEY_RIGHT);
    CHECK(vec2_eq(g.player.pos, vec2(20, 19)));
    ui_key(&ui, &g, 'S'); /* across the room to the far wall */
    CHECK(vec2_eq(g.player.pos, vec2(20, 24)));

    map_set(g.map, 15, 11, TILE_FLOOR); /* a fork */
    g.player.pos = vec2(10, 10);
    ui_key(&ui, &g, 'D');
    CHECK(vec2_eq(g.player.pos, vec2(15, 10)));

    map_set(g.map, 15, 11, TILE_WALL);
    level_add_item(game_level(&g), (Item){.cls = ITEM_FOOD, .count = 1}, vec2(13, 10));
    g.player.pos = vec2(10, 10);
    ui_key(&ui, &g, 'D');
    CHECK(vec2_eq(g.player.pos, vec2(13, 10)));
    game_free(&g);
}

/* With a monster in view a run is one step; confused, too. */
static void runs_are_single_steps_near_monsters(void)
{
    Game g;
    Ui ui;
    corridor_game(&g);
    ui_init(&ui, NULL, NULL);
    Monster m = monster_make(MON_RAT, vec2(18, 10), &g.rng);
    level_add_monster(game_level(&g), m);
    game_look(&g);
    ui_key(&ui, &g, 'D');
    CHECK(vec2_eq(g.player.pos, vec2(11, 10)));
    game_free(&g);
}

/* Real time: a run takes a step whenever the player is ready; a key stops it. */
static void real_time_runs_step_by_step(void)
{
    Game g;
    Ui ui;
    corridor_game(&g);
    ui_init(&ui, NULL, NULL);
    game_set_realtime(&g, true);
    ui_key(&ui, &g, 'D');
    CHECK(vec2_eq(g.player.pos, vec2(11, 10)) && game_run_active(&g));
    ui_update(&ui, &g, 3 * UI_TICK_MS);
    CHECK(vec2_eq(g.player.pos, vec2(14, 10)) && game_run_active(&g));
    ui_key(&ui, &g, 'a'); /* stops the run; does not move */
    CHECK(!game_run_active(&g) && vec2_eq(g.player.pos, vec2(14, 10)));
    ui_update(&ui, &g, 2 * UI_TICK_MS);
    CHECK(vec2_eq(g.player.pos, vec2(14, 10)));
    game_free(&g);
}

/* q saves (and quits where it can); Q asks, then gives the game up. A new
 * depth asks for a save, the end of the game for deleting it. */
static void saving_quitting_and_giving_up(void)
{
    Game g;
    Ui ui;
    open_game(&g);
    ui_init(&ui, NULL, NULL); /* a browser tab: can't exit */
    ui_key(&ui, &g, 'q');
    CHECK(g.running && ui.save_wanted);
    CHECK(strcmp(msglog_get(&g.log, 0)->text, "Game saved. Closing the tab saves it too.") == 0);

    ui.save_wanted = false;
    ui_key(&ui, &g, 'Q');
    ui_render(&ui, &g, cells, COLS, ROWS);
    CHECK(ui.confirm_abandon && row_has(1, 0, "Give up this game?"));
    ui_key(&ui, &g, 'n');
    CHECK(!ui.confirm_abandon && !ui.save_delete && !ui.new_game && g.turn == 0);
    ui_key(&ui, &g, 'Q');
    ui_key(&ui, &g, 'y');
    CHECK(ui.save_delete && ui.new_game && !ui.save_wanted);
    game_free(&g);

    game_init(&g, 21);
    clear_monsters(&g);
    ui_init(&ui, NULL, NULL);
    ui.can_exit = true;
    g.player.pos = g.levels[0].stairs_down;
    ui_key(&ui, &g, '>');
    CHECK(g.depth == 2 && ui.save_wanted);
    ui.save_wanted = false;
    ui_key(&ui, &g, '.');
    CHECK(!ui.save_wanted);
    game_kill_player(&g, DEATH_STARVATION);
    ui_key(&ui, &g, '.');
    CHECK(ui.screen == SCREEN_END && ui.save_delete && !ui.save_wanted);
    game_free(&g);

    open_game(&g);
    ui_init(&ui, NULL, NULL);
    ui.can_exit = true;
    ui_key(&ui, &g, 'Q');
    ui_key(&ui, &g, 'y');
    CHECK(!g.running && ui.save_delete && !ui.new_game);
    game_free(&g);
}

/* o opens the options; choices change the settings and the map screen. */
static void options_change_settings(void)
{
    Game g;
    Ui ui;
    Settings settings = settings_default();
    open_game(&g);
    ui_init(&ui, NULL, &settings);

    ui_key(&ui, &g, 'o');
    ui_render(&ui, &g, cells, COLS, ROWS);
    CHECK(ui.screen == SCREEN_OPTIONS && screen_has("Real-time speed") && !screen_has("Colours"));
    ui_key(&ui, &g, '\n');
    CHECK(settings.realtime_start && ui.settings_changed && !g.realtime);
    ui_key(&ui, &g, UI_KEY_DOWN);
    ui_key(&ui, &g, UI_KEY_RIGHT);
    CHECK(settings.speed == SPEED_FAST);
    ui_key(&ui, &g, UI_KEY_RIGHT); /* wraps around */
    CHECK(settings.speed == SPEED_SLOW);
    ui_key(&ui, &g, UI_KEY_DOWN);
    ui_key(&ui, &g, UI_KEY_LEFT); /* 2 -> 4 */
    CHECK(settings.log_lines == 4);
    ui_key(&ui, &g, UI_KEY_DOWN); /* no colour line: back to the top */
    CHECK(ui.option_row == 0);
    ui_key(&ui, &g, 27);
    CHECK(ui.screen == SCREEN_MAP && g.turn == 0);

    for (int i = 0; i < 4; i++)
        msglog_add(&g.log, 0, "Line %d", i);
    ui_render(&ui, &g, cells, COLS, ROWS);
    CHECK(row_has(1, 0, "Line 0") && row_has(1, 3, "Line 3"));

    ui.colour_option = true;
    ui_key(&ui, &g, 'o');
    ui_key(&ui, &g, UI_KEY_UP); /* wraps to the last line: colours */
    ui_key(&ui, &g, UI_KEY_RIGHT);
    CHECK(settings.colours == COLOURS_FULL);
    game_free(&g);
}

void run_ui_tests(void)
{
    RUN(runs_follow_corridors_and_stop);
    RUN(runs_are_single_steps_near_monsters);
    RUN(real_time_runs_step_by_step);
    RUN(saving_quitting_and_giving_up);
    RUN(options_change_settings);
    RUN(keys_move_on_the_map_screen);
    RUN(help_opens_and_any_key_closes);
    RUN(history_shows_messages_and_scrolls);
    RUN(screens_survive_tiny_sizes);
}
