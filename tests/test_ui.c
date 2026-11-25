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
    ui_init(&ui);

    ui_key(&ui, &g, 'd');
    CHECK(vec2_eq(g.player.pos, vec2(51, 20)));
    ui_key(&ui, &g, UI_KEY_UP);
    CHECK(vec2_eq(g.player.pos, vec2(51, 19)));
    ui_key(&ui, &g, UI_KEY_PAGE_DOWN); /* south-east */
    CHECK(vec2_eq(g.player.pos, vec2(52, 20)));
    ui_key(&ui, &g, UI_KEY_CENTER);
    CHECK(g.turn == 4);
    ui_key(&ui, &g, 'q');
    CHECK(!g.running);
    game_free(&g);
}

static void help_opens_and_any_key_closes(void)
{
    Game g;
    Ui ui;
    open_game(&g);
    ui_init(&ui);

    ui_key(&ui, &g, '?');
    CHECK(ui.screen == SCREEN_HELP);
    ui_render(&ui, &g, cells, COLS, ROWS);
    CHECK(screen_has("Help") && screen_has("Message history") && screen_has("Quit"));
    CHECK(!screen_has("Seed")); /* not the map screen */

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
    ui_init(&ui);
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
    open_game(&g);
    for (Screen s = SCREEN_MAP; s <= SCREEN_LOG; s++)
        for (int c = 0; c <= 3; c++)
            for (int r = 0; r <= 3; r++)
            {
                ui = (Ui){.screen = s, .scroll = 5};
                ui_render(&ui, &g, cells, c, r);
            }
    CHECK(true); /* no crash, no sanitizer report */
    game_free(&g);
}

void run_ui_tests(void)
{
    RUN(keys_move_on_the_map_screen);
    RUN(help_opens_and_any_key_closes);
    RUN(history_shows_messages_and_scrolls);
    RUN(screens_survive_tiny_sizes);
}
