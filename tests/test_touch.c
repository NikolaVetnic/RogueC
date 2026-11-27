#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "core/path.h"
#include "core/ui.h"

/* A phone held upright: about 30 x 38 cells. */
enum
{
    COLS = 30,
    ROWS = 38,
    WIDE = 80,
};
static Cell cells[WIDE * ROWS];

/* Finds text on a cols-wide screen, the lowest first (buttons are at the
 * bottom); its first cell in *x, *y. */
static bool find(int cols, int rows, const char *text, int *x, int *y)
{
    int n = (int)strlen(text);
    for (int r = rows - 1; r >= 0; r--)
        for (int c = 0; c + n <= cols; c++)
        {
            int i = 0;
            while (i < n && cells[r * cols + c + i].glyph == (unsigned char)text[i])
                i++;
            if (i == n)
            {
                *x = c;
                *y = r;
                return true;
            }
        }
    return false;
}

static bool has(int cols, int rows, const char *text)
{
    int x, y;
    return find(cols, rows, text, &x, &y);
}

/* Draws the screen, then taps the label `text` on it. */
static bool tap_label(Ui *ui, Game *g, const char *text)
{
    int x, y;
    ui_render(ui, g, cells, COLS, ROWS);
    if (!find(COLS, ROWS, text, &x, &y))
        return false;
    ui_tap(ui, g, x, y);
    return true;
}

/* The screen cell showing map cell `at` (after a ui_render). */
static void screen_of(const Ui *ui, Vec2 at, int *x, int *y)
{
    *x = at.x - ui->map_layout.camera.x;
    *y = at.y - ui->map_layout.camera.y + ui->map_layout.log_rows;
}

static int steps_between(Vec2 a, Vec2 b)
{
    int dx = abs(a.x - b.x), dy = abs(a.y - b.y);
    return dx > dy ? dx : dy;
}

static void explore_all(Game *g)
{
    memset(g->map->explored, 1, (size_t)g->map->w * (size_t)g->map->h * sizeof *g->map->explored);
}

/* Rock with a corridor from (10,10) east to (20,10), then south into a room
 * (15-25, 19-24) through a gap at (20,18); all of it explored. */
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
    explore_all(g);
    game_look(g);
}

static int known_steps(const Game *g, Vec2 from, Vec2 to)
{
    int *dist = malloc((size_t)g->map->w * (size_t)g->map->h * sizeof *dist);
    path_distances_known(g->map, from, dist);
    int d = dist[to.y * g->map->w + to.x];
    free(dist);
    return d;
}

static void known_distances_skip_the_unknown(void)
{
    Game g;
    corridor_game(&g);
    CHECK(known_steps(&g, vec2(10, 10), vec2(22, 22)) == 21);
    g.map->explored[10 * g.map->w + 15] = false; /* a gap in what the player knows */
    CHECK(known_steps(&g, vec2(10, 10), vec2(22, 22)) == -1);
    int *dist = malloc((size_t)g.map->w * (size_t)g.map->h * sizeof *dist);
    path_distances(g.map, vec2(10, 10), dist);
    CHECK(dist[22 * g.map->w + 22] == 21);
    free(dist);
    game_free(&g);
}

/* Travel takes a shortest known way and stops there; not into the unknown. */
static void travel_walks_known_ways(void)
{
    Game g;
    corridor_game(&g);
    CHECK(game_travel(&g, vec2(22, 22)));
    CHECK(vec2_eq(g.player.pos, vec2(22, 22)) && g.turn == 21 && !game_run_active(&g));

    CHECK(!game_travel(&g, vec2(22, 22))); /* already there */
    g.map->explored[10 * g.map->w + 15] = false;
    CHECK(!game_travel(&g, vec2(10, 10)) && g.turn == 21);
    CHECK(strcmp(msglog_get(&g.log, 0)->text, "You know no way there.") == 0);
    CHECK(!game_travel(&g, vec2(0, 0))); /* rock */
    game_free(&g);
}

/* A monster coming into view stops it; with one in view each travel is one step. */
static void travel_stops_for_monsters(void)
{
    Game g;
    corridor_game(&g);
    level_add_monster(game_level(&g), monster_make(MON_RAT, vec2(25, 24), &g.rng));
    game_look(&g);
    Vec2 target = vec2(15, 24);
    game_travel(&g, target);
    CHECK(!vec2_eq(g.player.pos, target) && map_is_visible(g.map, 25, 24));
    int turn = g.turn;
    Vec2 before = g.player.pos;
    game_travel(&g, target);
    CHECK(g.turn == turn + 1 && steps_between(before, g.player.pos) == 1);
    game_free(&g);
}

/* Real time: a step whenever the player is ready; a tap stops it. */
static void real_time_travel_and_taps_stop_it(void)
{
    Game g;
    Ui ui;
    corridor_game(&g);
    ui_init(&ui, NULL, NULL);
    game_set_realtime(&g, true);
    ui_render(&ui, &g, cells, WIDE, ROWS);
    int x, y;
    screen_of(&ui, vec2(20, 10), &x, &y);
    ui_tap(&ui, &g, x, y);
    CHECK(vec2_eq(g.player.pos, vec2(11, 10)) && game_run_active(&g));
    ui_update(&ui, &g, 3 * UI_TICK_MS);
    CHECK(vec2_eq(g.player.pos, vec2(14, 10)));
    ui_tap(&ui, &g, 0, 0); /* anywhere: stops, does nothing else */
    CHECK(!game_run_active(&g) && ui.screen == SCREEN_MAP);
    ui_update(&ui, &g, 2 * UI_TICK_MS);
    CHECK(vec2_eq(g.player.pos, vec2(14, 10)));
    game_free(&g);
}

/* On the map: yourself waits or picks up, a neighbour steps or attacks,
 * further walks there (a near miss on rock goes to the floor beside it), the
 * messages open the history. A click works the same without touch buttons. */
static void map_taps(void)
{
    Game g;
    Ui ui;
    corridor_game(&g);
    ui_init(&ui, NULL, NULL);
    int x, y;

    ui_render(&ui, &g, cells, WIDE, ROWS);
    screen_of(&ui, g.player.pos, &x, &y);
    ui_tap(&ui, &g, x, y);
    CHECK(g.turn == 1 && vec2_eq(g.player.pos, vec2(10, 10)));
    ui_tap(&ui, &g, x + 1, y); /* east */
    CHECK(vec2_eq(g.player.pos, vec2(11, 10)) && g.turn == 2);
    ui_tap(&ui, &g, x, y - 1); /* rock: no turn */
    CHECK(vec2_eq(g.player.pos, vec2(11, 10)) && g.turn == 2);

    ui_render(&ui, &g, cells, WIDE, ROWS);
    screen_of(&ui, vec2(18, 9), &x, &y); /* rock above the corridor */
    ui_tap(&ui, &g, x, y);
    CHECK(vec2_eq(g.player.pos, vec2(17, 10)));

    level_add_item(game_level(&g), (Item){.cls = ITEM_SCROLL, .count = 1}, vec2(17, 10));
    int pack = g.player.pack_count;
    ui_render(&ui, &g, cells, WIDE, ROWS);
    screen_of(&ui, g.player.pos, &x, &y);
    ui_tap(&ui, &g, x, y);
    CHECK(g.player.pack_count == pack + 1);

    ui_tap(&ui, &g, 3, 0); /* the messages */
    CHECK(ui.screen == SCREEN_LOG);
    game_free(&g);
}

/* Touch: buttons along the bottom, none of the key hints; Items, a tap on an
 * item, Back. */
static void touch_buttons_and_lists(void)
{
    Game g;
    Ui ui;
    corridor_game(&g);
    ui_init(&ui, NULL, NULL);
    ui.touch = true;
    ui_render(&ui, &g, cells, COLS, ROWS);
    int x, y;
    CHECK(find(COLS, ROWS, "Menu", &x, &y) && y == ROWS - 2);
    CHECK(has(COLS, ROWS, "Rest") && has(COLS, ROWS, "Drop") && !has(COLS, ROWS, "? help"));
    CHECK(cells[(ROWS - 1) * COLS].style == STYLE_BUTTON);
    CHECK(has(COLS, ROWS, "HP 20/20")); /* the status line fits */

    CHECK(tap_label(&ui, &g, "Items") || tap_label(&ui, &g, "Inv"));
    CHECK(ui.screen == SCREEN_PACK);
    ui_render(&ui, &g, cells, COLS, ROWS);
    CHECK(has(COLS, ROWS, "Tap an item to use it."));
    CHECK(tap_label(&ui, &g, "c -")); /* the food */
    CHECK(ui.screen == SCREEN_MAP && g.player.pack_count == 2 && g.turn == 1);

    ui_key(&ui, &g, 'x');
    CHECK(tap_label(&ui, &g, "Back") && ui.screen == SCREEN_MAP && g.player.pack_count == 2);
    CHECK(ui.target_count == 0); /* stale until drawn again */

    ui_key(&ui, &g, 'm');
    CHECK(tap_label(&ui, &g, "Older") && ui.screen == SCREEN_LOG);
    CHECK(tap_label(&ui, &g, "Back") && ui.screen == SCREEN_MAP);
    game_free(&g);
}

/* The menu: options (a tap changes one), real time, save, give up. */
static void touch_menu(void)
{
    Game g;
    Ui ui;
    Settings settings = settings_default();
    corridor_game(&g);
    ui_init(&ui, NULL, &settings);
    ui.touch = true;

    CHECK(tap_label(&ui, &g, "Menu") && ui.screen == SCREEN_MENU);
    CHECK(tap_label(&ui, &g, "Options") && ui.screen == SCREEN_OPTIONS);
    ui_render(&ui, &g, cells, COLS, ROWS);
    CHECK(has(COLS, ROWS, "off") && has(COLS, ROWS, "normal: a turn")); /* values under names */
    CHECK(tap_label(&ui, &g, "Real-time speed"));
    CHECK(settings.speed == SPEED_FAST && ui.option_row == 1 && ui.settings_changed);
    CHECK(tap_label(&ui, &g, "Back") && ui.screen == SCREEN_MAP);

    tap_label(&ui, &g, "Menu");
    CHECK(tap_label(&ui, &g, "Switch to real time") && g.realtime && ui.screen == SCREEN_MAP);
    tap_label(&ui, &g, "Menu");
    CHECK(tap_label(&ui, &g, "Save") && ui.save_wanted && g.running);

    tap_label(&ui, &g, "Menu");
    CHECK(tap_label(&ui, &g, "Give up this game") && ui.confirm_abandon);
    ui_render(&ui, &g, cells, COLS, ROWS);
    CHECK(has(COLS, ROWS, "Give up this game?"));
    CHECK(tap_label(&ui, &g, "Keep playing") && !ui.confirm_abandon && !ui.new_game);
    tap_label(&ui, &g, "Menu");
    tap_label(&ui, &g, "Give up this game");
    ui_render(&ui, &g, cells, COLS, ROWS);
    ui_tap(&ui, &g, COLS / 2, ROWS / 2); /* not an answer: no */
    CHECK(!ui.confirm_abandon && !ui.new_game);
    tap_label(&ui, &g, "Menu");
    tap_label(&ui, &g, "Give up this game");
    CHECK(tap_label(&ui, &g, "Give up") && ui.new_game && ui.save_delete);

    ui_key(&ui, &g, UI_KEY_MENU); /* keys work on the menu too: any other goes back */
    ui_key(&ui, &g, 'z');
    CHECK(ui.screen == SCREEN_MAP);
    game_free(&g);
}

/* Stairs: walks to the known stairs, then takes them. */
static void stairs_button(void)
{
    Game g;
    Ui ui;
    game_init(&g, 21);
    clear_monsters(&g);
    game_level(&g)->item_count = 0;
    ui_init(&ui, NULL, NULL);
    ui_key(&ui, &g, UI_KEY_STAIRS); /* on the stairs up, but they lead the wrong way */
    CHECK(g.depth == 1 && g.turn == 0 &&
          strcmp(msglog_get(&g.log, 0)->text, "You haven't found the stairs down here yet.") == 0);
    explore_all(&g);
    ui_key(&ui, &g, UI_KEY_STAIRS);
    CHECK(vec2_eq(g.player.pos, g.levels[0].stairs_down) && g.depth == 1);
    ui_key(&ui, &g, UI_KEY_STAIRS);
    CHECK(g.depth == 2 && ui.save_wanted);

    ui_render(&ui, &g, cells, WIDE, ROWS); /* tapping yourself on the stairs up takes them */
    int x, y;
    screen_of(&ui, g.player.pos, &x, &y);
    ui_tap(&ui, &g, x, y);
    CHECK(g.depth == 1);
    game_free(&g);
}

/* Narrow screens: long messages wrap, the status line shortens to keep the
 * warnings, the end screen wraps and offers a button. */
static void narrow_screens(void)
{
    Game g;
    Ui ui;
    Settings settings = settings_default();
    settings.log_lines = 4;
    corridor_game(&g);
    ui_init(&ui, NULL, &settings);
    ui.touch = true;
    msglog_add(&g.log, 0, "Real time: the dungeon moves on its own. T to stop.");
    ui_render(&ui, &g, cells, COLS, ROWS);
    CHECK(has(COLS, ROWS, "Real time: the dungeon moves") &&
          has(COLS, ROWS, "  on its own. T to stop."));
    CHECK(has(COLS, ROWS, "You enter the dungeon.")); /* the message before it */

    g.player.nutrition = 50;
    g.player.confused = 5;
    ui_render(&ui, &g, cells, COLS, ROWS);
    CHECK(has(COLS, ROWS, "Weak") && has(COLS, ROWS, "Confused"));

    ui_key(&ui, &g, '?');
    ui_render(&ui, &g, cells, COLS, ROWS);
    CHECK(has(COLS, ROWS, "Tap a place") && !has(COLS, ROWS, "w a s d"));
    ui_tap(&ui, &g, 5, 5); /* anywhere goes back */
    CHECK(ui.screen == SCREEN_MAP);

    game_kill_player(&g, DEATH_STARVATION);
    ui_key(&ui, &g, '.');
    CHECK(ui.screen == SCREEN_END);
    ui_render(&ui, &g, cells, COLS, ROWS);
    CHECK(has(COLS, ROWS, "experience") && !has(COLS, ROWS, "Enter"));
    CHECK(tap_label(&ui, &g, "New game") && ui.new_game);
    game_free(&g);
}

static void touch_screens_survive_tiny_sizes(void)
{
    Game g;
    Ui ui;
    Settings settings = settings_default();
    corridor_game(&g);
    for (Screen s = SCREEN_MAP; s <= SCREEN_MENU; s++)
        for (int c = 0; c <= 12; c += 3)
            for (int r = 0; r <= 12; r++)
            {
                ui_init(&ui, NULL, &settings);
                ui.screen = s;
                ui.touch = true;
                ui.confirm_abandon = r % 2 == 0;
                ui_render(&ui, &g, cells, c, r);
                ui_tap(&ui, &g, c / 2, r / 2);
            }
    CHECK(true); /* no crash, no sanitizer report */
    game_free(&g);
}

void run_touch_tests(void)
{
    RUN(known_distances_skip_the_unknown);
    RUN(travel_walks_known_ways);
    RUN(travel_stops_for_monsters);
    RUN(real_time_travel_and_taps_stop_it);
    RUN(map_taps);
    RUN(touch_buttons_and_lists);
    RUN(touch_menu);
    RUN(stairs_button);
    RUN(narrow_screens);
    RUN(touch_screens_survive_tiny_sizes);
}
