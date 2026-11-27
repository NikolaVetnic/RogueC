#include "core/ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    LOG_PAGE = 10,    /* lines per Page Up / Page Down in the history */
    CTRL_P = 16,      /* message history, as in Rogue */
    MAX_CATCH_UP = 4, /* real time: ticks run at most per update (after a stall) */
};

void ui_init(Ui *ui, ScoreTable *scores, Settings *settings)
{
    *ui = (Ui){.screen = SCREEN_MAP, .scores = scores, .score_rank = -1, .settings = settings};
}

/* Moves; with Shift (capital letters, UI_KEY_SHIFT) they run. */
static Action move_for_key(int key)
{
    // clang-format off
    switch (key)
    {
    case 'w': case 'k': case '8': case UI_KEY_UP:        return ACTION_MOVE_N;
    case 's': case 'j': case '2': case UI_KEY_DOWN:      return ACTION_MOVE_S;
    case 'a': case 'h': case '4': case UI_KEY_LEFT:      return ACTION_MOVE_W;
    case 'd': case 'l': case '6': case UI_KEY_RIGHT:     return ACTION_MOVE_E;
    case 'y': case '7': case UI_KEY_HOME:                return ACTION_MOVE_NW;
    case 'u': case '9': case UI_KEY_PAGE_UP:             return ACTION_MOVE_NE;
    case 'b': case '1': case UI_KEY_END:                 return ACTION_MOVE_SW;
    case 'n': case '3': case UI_KEY_PAGE_DOWN:           return ACTION_MOVE_SE;
    default:                                             return ACTION_NONE;
    }
    // clang-format on
}

static Action action_for_key(int key)
{
    Action move = move_for_key(key);
    if (move != ACTION_NONE)
        return move;
    int plain = key & UI_KEY_SHIFT         ? key & ~UI_KEY_SHIFT
                : key >= 'A' && key <= 'Z' ? key - 'A' + 'a'
                                           : 0;
    move = plain ? move_for_key(plain) : ACTION_NONE;
    if (move != ACTION_NONE)
        return (Action)(ACTION_RUN_N + (move - ACTION_MOVE_N));

    // clang-format off
    switch (key)
    {
    case '.': case '5': case UI_KEY_CENTER:              return ACTION_WAIT;
    case 'Z':                                            return ACTION_REST;
    case '>':                                            return ACTION_DESCEND;
    case '<':                                            return ACTION_ASCEND;
    case ',': case 'g':                                  return ACTION_PICK_UP;
    default:                                             return ACTION_NONE;
    }
    // clang-format on
}

static void log_key(Ui *ui, const Game *g, int key)
{
    int max_scroll = msglog_len(&g->log) > 0 ? msglog_len(&g->log) - 1 : 0;

    // clang-format off
    switch (key)
    {
    case UI_KEY_UP:   case 'k': case 'w': case '8': ui->scroll++; break;
    case UI_KEY_DOWN: case 'j': case 's': case '2': ui->scroll--; break;
    case UI_KEY_PAGE_UP:   ui->scroll += LOG_PAGE; break;
    case UI_KEY_PAGE_DOWN: ui->scroll -= LOG_PAGE; break;
    case UI_KEY_HOME:      ui->scroll = max_scroll; break;
    case UI_KEY_END:       ui->scroll = 0; break;
    default:               ui->screen = SCREEN_MAP; break;
    }
    // clang-format on

    if (ui->scroll > max_scroll)
        ui->scroll = max_scroll;
    if (ui->scroll < 0)
        ui->scroll = 0;
}

/* Turn-based, or ready: act now. Real time and not ready: keep the latest
 * move for when the player is (quitting never waits). `item` is for use and
 * drop, `target` for travel. */
static void act_at(Ui *ui, Game *g, Action a, int item, Vec2 target)
{
    if (a == ACTION_NONE)
        return;
    if (g->realtime && !game_ready(g))
    {
        ui->pending = a;
        ui->pending_item = item;
        ui->pending_target = target;
    }
    else
    {
        ui->pending = ACTION_NONE;
        if (a == ACTION_TRAVEL)
            game_travel(g, target);
        else
            game_step_item(g, a, item);
    }
}

static void act(Ui *ui, Game *g, Action a, int item)
{
    act_at(ui, g, a, item, vec2(0, 0));
}

/* The stairs button, towards the goal: down, or up once carrying it. Walks to
 * the known stairs; on them, takes them. */
static void stairs(Ui *ui, Game *g)
{
    bool up = game_has_goal(g);
    Vec2 at = g->player.pos;
    if (map_get(g->map, at.x, at.y) == (up ? TILE_STAIRS_UP : TILE_STAIRS_DOWN))
    {
        act(ui, g, up ? ACTION_ASCEND : ACTION_DESCEND, -1);
        return;
    }
    Vec2 to = up ? game_level(g)->stairs_up : game_level(g)->stairs_down;
    if (to.x >= 0 && map_is_explored(g->map, to.x, to.y))
        act_at(ui, g, ACTION_TRAVEL, -1, to);
    else
        msglog_add(&g->log, g->turn, "You haven't found the stairs %s here yet.",
                   up ? "up" : "down");
}

/* A pack letter: use or drop that item, back to the map. Other keys: back. */
static void pack_key(Ui *ui, Game *g, int key)
{
    ui->screen = SCREEN_MAP;
    int i = key - 'a';
    if (i >= 0 && i < g->player.pack_count)
        act(ui, g, ui->pack_purpose == PACK_USE ? ACTION_USE : ACTION_DROP, i);
}

/* After anything that may have changed the game: a new depth asks for a
 * save; once the game is over, the end screen and the score into the table. */
static void check_game(Ui *ui, const Game *g)
{
    if (g->depth != ui->last_depth && !game_over(g))
        ui->save_wanted = true;
    ui->last_depth = g->depth;

    if (!game_over(g) || ui->screen == SCREEN_END)
        return;
    ui->screen = SCREEN_END;
    ui->save_delete = true;
    if (ui->scores)
    {
        ui->score_rank = scores_add(ui->scores, score_of(g));
        ui->scores_changed = true;
    }
}

/* q: save; and quit where the program can end. */
static void save_and_quit(Ui *ui, Game *g)
{
    ui->save_wanted = true;
    if (ui->can_exit)
        game_step(g, ACTION_QUIT);
    else
        msglog_add(&g->log, g->turn, "Game saved. Closing the tab saves it too.");
}

/* Q, then y: no save, no score. */
static void abandon(Ui *ui, Game *g)
{
    ui->save_delete = true;
    if (ui->can_exit)
        game_step(g, ACTION_QUIT);
    else
        ui->new_game = true;
}

/* ---- options ---------------------------------------------------------------- */

enum
{
    OPT_REALTIME,
    OPT_SPEED,
    OPT_LOG_LINES,
    OPT_COLOURS, /* only with ui->colour_option */
};

static int option_count(const Ui *ui)
{
    return ui->colour_option ? OPT_COLOURS + 1 : OPT_COLOURS;
}

/* Steps a value through [0, count), wrapping around. */
static int cycle(int value, int count, int step)
{
    return ((value + step) % count + count) % count;
}

static void change_option(Ui *ui, int step)
{
    Settings *s = ui->settings;
    switch (ui->option_row)
    {
    case OPT_REALTIME:
        s->realtime_start = !s->realtime_start;
        break;
    case OPT_SPEED:
        s->speed = (Speed)cycle((int)s->speed, SPEED_COUNT, step);
        break;
    case OPT_LOG_LINES:
        s->log_lines = LOG_LINES_MIN +
                       cycle(s->log_lines - LOG_LINES_MIN, LOG_LINES_MAX - LOG_LINES_MIN + 1, step);
        break;
    default:
        s->colours = (Colours)cycle((int)s->colours, COLOURS_COUNT, step);
        break;
    }
    ui->settings_changed = true;
}

static void options_key(Ui *ui, int key)
{
    if (key >= UI_KEY_ROW && key < UI_KEY_ROW + option_count(ui)) /* a tap on a line */
    {
        ui->option_row = key - UI_KEY_ROW;
        change_option(ui, 1);
        return;
    }
    // clang-format off
    switch (key)
    {
    case UI_KEY_UP:   case 'k': case 'w': case '8':
        ui->option_row = cycle(ui->option_row, option_count(ui), -1); break;
    case UI_KEY_DOWN: case 'j': case 's': case '2':
        ui->option_row = cycle(ui->option_row, option_count(ui), 1); break;
    case UI_KEY_LEFT: case 'h': case 'a': case '4':
        change_option(ui, -1); break;
    case UI_KEY_RIGHT: case 'l': case 'd': case '6': case '\n': case ' ':
        change_option(ui, 1); break;
    default:
        ui->screen = SCREEN_MAP; break;
    }
    // clang-format on
}

/* ---- the touch menu ------------------------------------------------------- */

typedef struct
{
    int key; /* the map screen's key for it */
    const char *label;
} MenuEntry;

static int menu_entries(const Ui *ui, const Game *g, MenuEntry out[6])
{
    int n = 0;
    out[n++] = (MenuEntry){'?', "Help"};
    out[n++] = (MenuEntry){'m', "Message history"};
    if (ui->settings)
        out[n++] = (MenuEntry){'o', "Options"};
    out[n++] = (MenuEntry){'T', g->realtime ? "Switch to turn-based" : "Switch to real time"};
    out[n++] = (MenuEntry){'q', ui->can_exit ? "Save and quit" : "Save"};
    out[n++] = (MenuEntry){'Q', "Give up this game"};
    return n;
}

/* An entry's key does what it does on the map screen; any other key goes back. */
static void menu_key(Ui *ui, Game *g, int key)
{
    MenuEntry e[6];
    int n = menu_entries(ui, g, e);
    ui->screen = SCREEN_MAP;
    for (int i = 0; i < n; i++)
        if (e[i].key == key)
            ui_key(ui, g, key);
}

void ui_key(Ui *ui, Game *g, int key)
{
    if (ui->last_depth == 0)
        ui->last_depth = g->depth;
    switch (ui->screen)
    {
    case SCREEN_MAP:
        if (game_run_active(g)) /* any key stops a run, and only that */
            game_run_stop(g);
        else if (ui->confirm_abandon)
        {
            ui->confirm_abandon = false;
            if (key == 'y' || key == 'Y')
                abandon(ui, g);
        }
        else if (key == '?')
            ui->screen = SCREEN_HELP;
        else if (key == 'm' || key == 'M' || key == CTRL_P)
        {
            ui->screen = SCREEN_LOG;
            ui->scroll = 0;
        }
        else if (key == 'i' || key == 'x')
        {
            ui->screen = SCREEN_PACK;
            ui->pack_purpose = key == 'i' ? PACK_USE : PACK_DROP;
        }
        else if (key == 'o' && ui->settings)
        {
            ui->screen = SCREEN_OPTIONS;
            ui->option_row = 0;
        }
        else if (key == UI_KEY_MENU)
            ui->screen = SCREEN_MENU;
        else if (key == UI_KEY_STAIRS)
            stairs(ui, g);
        else if (key == 'q')
            save_and_quit(ui, g);
        else if (key == 'Q')
            ui->confirm_abandon = true;
        else if (key == 'T')
        {
            game_set_realtime(g, !g->realtime);
            ui->pending = ACTION_NONE;
            ui->clock_ms = 0;
        }
        else
            act(ui, g, action_for_key(key), -1);
        check_game(ui, g);
        break;
    case SCREEN_PACK:
        pack_key(ui, g, key);
        check_game(ui, g);
        break;
    case SCREEN_OPTIONS:
        options_key(ui, key);
        break;
    case SCREEN_MENU:
        menu_key(ui, g, key);
        break;
    case SCREEN_HELP:
        ui->screen = SCREEN_MAP; /* any key */
        break;
    case SCREEN_LOG:
        log_key(ui, g, key);
        break;
    case SCREEN_END:
        if (key == '\n' || key == ' ')
            ui->new_game = true;
        else if (key == 'q' || key == 'Q' || key == 27)
            g->running = false;
        break;
    }
}

/* ---- taps ------------------------------------------------------------------- */

static bool known_floor(const Map *m, Vec2 c)
{
    return map_walkable(m, c.x, c.y) && map_is_explored(m, c.x, c.y);
}

static int chebyshev(Vec2 a, Vec2 b)
{
    int dx = abs(a.x - b.x), dy = abs(a.y - b.y);
    return dx > dy ? dx : dy;
}

/* A tap on the map: on the player, pick up, take the stairs or wait; next to
 * the player, step or attack; further, walk there. */
static void map_tap(Ui *ui, Game *g, int x, int y)
{
    static const Vec2 DIRS[8] = {{0, -1}, {1, -1}, {1, 0},  {1, 1},
                                 {0, 1},  {-1, 1}, {-1, 0}, {-1, -1}}; /* as ACTION_MOVE_* */
    const MapLayout *l = &ui->map_layout;
    if (y < l->log_rows || y >= l->log_rows + l->map_rows)
        return;
    Vec2 cell = vec2(l->camera.x + x, l->camera.y + y - l->log_rows);
    Vec2 at = g->player.pos;

    if (vec2_eq(cell, at))
    {
        Tile here = map_get(g->map, at.x, at.y);
        act(ui, g,
            level_item_at(game_level(g), at) >= 0 ? ACTION_PICK_UP
            : here == TILE_STAIRS_DOWN            ? ACTION_DESCEND
            : here == TILE_STAIRS_UP              ? ACTION_ASCEND
                                                  : ACTION_WAIT,
            -1);
        return;
    }
    for (int d = 0; d < 8; d++)
        if (vec2_eq(cell, vec2_add(at, DIRS[d])))
        {
            act(ui, g, (Action)(ACTION_MOVE_N + d), -1);
            return;
        }

    /* A near miss (rock, the unknown): the known floor beside it nearest the player. */
    if (!known_floor(g->map, cell))
    {
        Vec2 best = cell;
        for (int d = 0; d < 8; d++)
        {
            Vec2 n = vec2_add(cell, DIRS[d]);
            if (known_floor(g->map, n) &&
                (vec2_eq(best, cell) || chebyshev(n, at) < chebyshev(best, at)))
                best = n;
        }
        cell = best;
    }
    act_at(ui, g, ACTION_TRAVEL, -1, cell);
}

void ui_tap(Ui *ui, Game *g, int x, int y)
{
    if (ui->last_depth == 0)
        ui->last_depth = g->depth;
    Screen before = ui->screen;
    const UiTarget *hit = NULL;
    for (int i = 0; i < ui->target_count; i++)
    {
        const UiTarget *t = &ui->targets[i];
        if (x >= t->x && x < t->x + t->w && y >= t->y && y < t->y + t->h)
            hit = t;
    }

    if (ui->screen == SCREEN_MAP && game_run_active(g))
        ui_key(ui, g, UI_KEY_OTHER); /* stops it, and only that */
    else if (hit)
        ui_key(ui, g, hit->key);
    else if (ui->screen == SCREEN_HELP)
        ui_key(ui, g, UI_KEY_OTHER);
    else if (ui->screen == SCREEN_MAP && ui->confirm_abandon)
        ui_key(ui, g, 'n');
    else if (ui->screen == SCREEN_MAP)
    {
        map_tap(ui, g, x, y);
        check_game(ui, g);
    }
    if (ui->screen != before)
        ui->target_count = 0; /* until the new screen is drawn */
}

void ui_update(Ui *ui, Game *g, int elapsed_ms)
{
    if (ui->last_depth == 0)
        ui->last_depth = g->depth;
    if (!g->realtime || ui->screen != SCREEN_MAP || g->dead)
    {
        ui->clock_ms = 0;
        return;
    }
    int tick_ms = ui->settings ? settings_tick_ms(ui->settings) : UI_TICK_MS;
    ui->clock_ms += elapsed_ms;
    if (ui->clock_ms > MAX_CATCH_UP * tick_ms)
        ui->clock_ms = MAX_CATCH_UP * tick_ms;

    for (;;)
    {
        if (game_run_active(g))
            game_run_continue(g);
        else if (ui->pending != ACTION_NONE && game_ready(g))
            act_at(ui, g, ui->pending, ui->pending_item, ui->pending_target);
        if (ui->clock_ms < tick_ms || g->dead)
            break;
        ui->clock_ms -= tick_ms;
        game_tick(g);
    }
    check_game(ui, g);
}

/* ---- touch buttons ------------------------------------------------------------ */

typedef struct
{
    const char *label;
    const char *short_label; /* when the label doesn't fit; NULL: cut it */
    int key;
} Button;

static void add_target(Ui *ui, int x, int y, int w, int h, int key)
{
    if (ui->target_count < UI_TARGETS_MAX && w > 0 && h > 0)
        ui->targets[ui->target_count++] = (UiTarget){.x = x, .y = y, .w = w, .h = h, .key = key};
}

/* Rows the button bar takes on a screen of `rows` (odd, so labels sit in the
 * middle); 0 without touch. */
static int bar_rows(const Ui *ui, int rows)
{
    if (!ui->touch || rows < 6)
        return 0;
    return rows >= 15 ? 3 : 1;
}

/* A w x h button with its label centred, and its tap target. */
static void render_button(Ui *ui, Cell *cells, int cols, int rows, int x, int y, int w, int h,
                          const Button *b)
{
    for (int r = y; r < y + h && r < rows; r++)
        for (int c = x; c < x + w && c < cols; c++)
            cells[r * cols + c] = (Cell){.glyph = ' ', .style = STYLE_BUTTON};
    const char *label = b->short_label && (int)strlen(b->label) > w ? b->short_label : b->label;
    char text[48];
    snprintf(text, sizeof text, "%.*s", w > 0 ? w : 0, label);
    render_text(cells, cols, rows, x + (w - (int)strlen(text)) / 2, y + (h - 1) / 2, text,
                STYLE_BUTTON);
    add_target(ui, x, y, w, h, b->key);
}

/* Buttons sharing the bottom h rows, one column apart. */
static void render_bar(Ui *ui, Cell *cells, int cols, int rows, int h, const Button *b, int n)
{
    if (h <= 0 || h > rows)
        return;
    render_clear(cells + (rows - h) * cols, cols, h, STYLE_DEFAULT);
    for (int i = 0; i < n; i++)
    {
        int x0 = i * (cols + 1) / n, x1 = (i + 1) * (cols + 1) / n - 1;
        render_button(ui, cells, cols, rows, x0, rows - h, x1 - x0, h, &b[i]);
    }
}

/* Lines per list entry: two (easier to tap) if they fit in `space` rows. */
static int entry_rows(const Ui *ui, int count, int space)
{
    return ui->touch && count * 2 <= space ? 2 : 1;
}

/* ---- screens -------------------------------------------------------------- */

static const struct
{
    const char *what;
    const char *keys;
} HELP_KEYS[] = {
    {"Move", "w a s d   arrow keys   h j k l   numpad"},
    {"Move diagonally", "y u b n   Home PgUp End PgDn   numpad 7 9 1 3"},
    {"Run", "Shift + a move key or arrow (W A S D, H J K L, Y U B N)"},
    {"Attack", "move into a monster"},
    {"Wait a turn", ".   numpad 5"},
    {"Rest until healed", "Z"},
    {"Pick up", ",   g"},
    {"Inventory: use an item", "i   then the item's letter"},
    {"Drop an item", "x   then the item's letter"},
    {"Stairs down / up", ">   <   (standing on them)"},
    {"Message history", "m   Ctrl-P"},
    {"Real time on / off", "T   (the dungeon moves on its own; help and history pause it)"},
    {"Options", "o"},
    {"Help", "?"},
    {"Save and quit", "q   (the game also saves itself on every new depth)"},
    {"Give up this game", "Q   (no save, no score)"},
};

static const struct
{
    const char *what;
    const char *does;
} HELP_TOUCH[] = {
    {"Tap a place", "walk there (stops when something happens)"},
    {"Tap next to you", "step there, or attack a monster"},
    {"Tap yourself", "pick up what is here, take the stairs, or wait a turn"},
    {"Tap the messages", "message history"},
    {"Items, Drop", "then tap the item"},
    {"Stairs", "walk to the stairs down (up with the Sunstone); again to take them"},
    {"Rest", "rest until healed"},
    {"Menu", "help, options, real time, save, give up"},
};

static const struct
{
    uint32_t glyph;
    Style style;
    const char *what;
} HELP_LEGEND[] = {
    {GLYPH_PLAYER, STYLE_PLAYER, "you"},
    {GLYPH_FLOOR, STYLE_FLOOR_LIT, "floor in view"},
    {GLYPH_FLOOR, STYLE_FLOOR_SEEN, "floor remembered"},
    {GLYPH_WALL, STYLE_WALL_LIT, "rock in view"},
    {GLYPH_WALL, STYLE_WALL_SEEN, "rock remembered"},
    {GLYPH_STAIRS_DOWN, STYLE_STAIRS_LIT, "stairs down"},
    {GLYPH_STAIRS_UP, STYLE_STAIRS_LIT, "stairs up"},
};

static void render_help(const Ui *ui, const Game *g, Cell *cells, int cols, int rows)
{
    enum
    {
        LEFT = 2,
        KEYS_X = 27, /* column of the key names */
    };
    int y = 1;

    render_clear(cells, cols, rows, STYLE_DEFAULT);
    render_text(cells, cols, rows, LEFT, y, "Help", STYLE_HEADING);
    y += 2;
    y = render_wrapped(
        cells, cols, rows, LEFT, y,
        "Goal: find the Sunstone (*) on depth 10 and carry it out by the stairs up on depth 1.",
        STYLE_TEXT);
    y++;
    if (ui->touch) /* what taps and buttons do; side by side if there is room */
        for (size_t i = 0; i < sizeof HELP_TOUCH / sizeof HELP_TOUCH[0]; i++, y++)
        {
            render_text(cells, cols, rows, LEFT, y, HELP_TOUCH[i].what, STYLE_KEY);
            if (cols >= KEYS_X + 30)
                render_text(cells, cols, rows, KEYS_X, y, HELP_TOUCH[i].does, STYLE_TEXT);
            else
                y = render_wrapped(cells, cols, rows, LEFT + 2, y + 1, HELP_TOUCH[i].does,
                                   STYLE_TEXT) -
                    1;
        }
    else
        for (size_t i = 0; i < sizeof HELP_KEYS / sizeof HELP_KEYS[0]; i++, y++)
        {
            const char *what = HELP_KEYS[i].what;
            if (!ui->can_exit && strcmp(what, "Save and quit") == 0)
                what = "Save (closing the tab saves too)";
            render_text(cells, cols, rows, LEFT, y, what, STYLE_TEXT);
            render_text(cells, cols, rows, KEYS_X, y, HELP_KEYS[i].keys, STYLE_KEY);
        }

    y++;
    render_text(cells, cols, rows, LEFT, y++, "Map", STYLE_HEADING);
    for (size_t i = 0; i < sizeof HELP_LEGEND / sizeof HELP_LEGEND[0]; i++, y++)
    {
        if (y < rows && LEFT < cols)
            cells[y * cols + LEFT] =
                (Cell){.glyph = HELP_LEGEND[i].glyph, .style = (uint8_t)HELP_LEGEND[i].style};
        render_text(cells, cols, rows, LEFT + 4, y, HELP_LEGEND[i].what, STYLE_TEXT);
    }

    /* Monsters, weakest first, in their colours. */
    for (int k = 0; k < MON_COUNT; k++)
        if (y < rows && LEFT + 2 * k < cols)
            cells[y * cols + LEFT + 2 * k] = (Cell){
                .glyph = MONSTERS[k].glyph, .style = (uint8_t)render_monster_style((MonsterKind)k)};
    render_text(cells, cols, rows, LEFT + 2 * MON_COUNT + 2, y++, "monsters, deeper ones tougher",
                STYLE_TEXT);

    /* Items: glyph and word. */
    static const char *const ITEM_WORDS[ITEM_CLASS_COUNT] = {
        "gold", "food", "potion", "scroll", "weapon", "armour", "the Sunstone: your goal"};
    int x = LEFT;
    for (int c = 0; c < ITEM_CLASS_COUNT; c++)
    {
        if (y < rows && x < cols)
            cells[y * cols + x] = (Cell){.glyph = item_glyph((ItemClass)c),
                                         .style = (uint8_t)(c == ITEM_GOLD   ? STYLE_GOLD
                                                            : c == ITEM_GOAL ? STYLE_GOAL
                                                                             : STYLE_ITEM)};
        x = render_text(cells, cols, rows, x + 2, y, ITEM_WORDS[c], STYLE_TEXT) + 3;
    }
    y++;

    char seed[48];
    snprintf(seed, sizeof seed, "Seed %llu", (unsigned long long)g->seed);
    render_text(cells, cols, rows, LEFT, y + 1, seed, STYLE_TEXT_DIM);
    render_text(cells, cols, rows, LEFT, y + 2,
                ui->touch ? "Tap to go back." : "Press any key to go back.", STYLE_TEXT_DIM);
}

/* Messages oldest first, the newest last; ui->scroll lines up from it. */
static void render_history(Ui *ui, const Game *g, Cell *cells, int cols, int rows)
{
    enum
    {
        LEFT = 2,
        TOP = 3, /* heading, hint, column names */
    };
    int lines = rows - TOP;
    int count = msglog_len(&g->log);
    int max_scroll = count > lines ? count - lines : 0;
    if (ui->scroll > max_scroll)
        ui->scroll = max_scroll;

    render_clear(cells, cols, rows, STYLE_DEFAULT);
    render_text(cells, cols, rows, LEFT, 0, "Message history", STYLE_HEADING);
    render_text(cells, cols, rows, LEFT, 1,
                ui->touch ? "Older and Newer scroll."
                          : "Up/Down, PgUp/PgDn, Home/End scroll; any other key goes back.",
                STYLE_TEXT_DIM);

    render_text(cells, cols, rows, LEFT, TOP - 1, " Turn  Message", STYLE_TEXT_DIM);
    int shown = count < lines ? count : lines; /* a short history starts at the top */
    for (int line = 0; line < shown; line++)
    {
        const Message *m = msglog_get(&g->log, ui->scroll + shown - 1 - line);
        if (!m)
            continue;
        char text[MSG_TEXT_SIZE + 32];
        char body[MSG_TEXT_SIZE + 16];
        render_message_text(m, body, sizeof body);
        snprintf(text, sizeof text, "%5d  %s", m->turn, body);
        render_text(cells, cols, rows, LEFT, TOP + line, text, STYLE_TEXT);
    }
}

/* Died or won: what happened, the score, the high-score table. */
static void render_end(const Ui *ui, const Game *g, Cell *cells, int cols, int rows)
{
    enum
    {
        LEFT = 2,
    };
    char text[128];
    Score s = score_of(g);

    render_clear(cells, cols, rows, STYLE_DEFAULT);
    render_text(cells, cols, rows, LEFT, 1, g->won ? "You won!" : "You died.", STYLE_HEADING);
    if (s.cause[0] >= 'a' && s.cause[0] <= 'z')
        s.cause[0] = (char)(s.cause[0] - 'a' + 'A');
    snprintf(text, sizeof text, "%s, turn %d, at level %d.", s.cause, g->turn, g->player.level);
    int y = render_wrapped(cells, cols, rows, LEFT, 3, text, STYLE_TEXT);
    snprintf(text, sizeof text, "Score %d: %d gold + 10 x %d experience + 100 x depth %d%s.",
             s.points, g->player.gold, g->player.xp, g->deepest,
             g->won ? " + 2000 for the win" : "");
    y = render_wrapped(cells, cols, rows, LEFT, y, text, STYLE_TEXT);
    snprintf(text, sizeof text, "Seed %llu", (unsigned long long)g->seed);
    render_text(cells, cols, rows, LEFT, y++, text, STYLE_TEXT_DIM);

    if (!ui->touch) /* touch: buttons instead */
    {
        int x = render_text(cells, cols, rows, LEFT, y + 1, "Enter", STYLE_KEY);
        x = render_text(cells, cols, rows, x, y + 1, ": new game    ", STYLE_TEXT);
        x = render_text(cells, cols, rows, x, y + 1, "q", STYLE_KEY);
        render_text(cells, cols, rows, x, y + 1, ": quit", STYLE_TEXT);
        y += 2;
    }

    if (!ui->scores || ui->scores->count == 0)
        return;
    int table_top = y + 1;
    render_text(cells, cols, rows, LEFT, table_top, "High scores", STYLE_HEADING);
    for (int i = 0; i < ui->scores->count; i++)
    {
        const Score *e = &ui->scores->entries[i];
        snprintf(text, sizeof text, "%2d. %6d  level %2d  %s", i + 1, e->points, e->level,
                 e->cause);
        render_text(cells, cols, rows, LEFT, table_top + 2 + i, text,
                    i == ui->score_rank ? STYLE_HEADING : STYLE_TEXT);
    }
}

static void render_pack(Ui *ui, const Game *g, Cell *cells, int cols, int rows)
{
    enum
    {
        LEFT = 2,
        TOP = 3,
    };
    const Player *p = &g->player;
    render_clear(cells, cols, rows, STYLE_DEFAULT);
    render_text(cells, cols, rows, LEFT, 0,
                ui->pack_purpose == PACK_USE ? "Inventory: use which item?" : "Drop which item?",
                STYLE_HEADING);
    const char *hint =
        ui->touch
            ? (ui->pack_purpose == PACK_USE ? "Tap an item to use it." : "Tap an item to drop it.")
        : ui->pack_purpose == PACK_USE ? "Its letter eats, drinks, reads, wields, wears, or takes "
                                         "off; any other key goes back."
                                       : "Its letter drops it; any other key goes back.";
    render_text(cells, cols, rows, LEFT, 1, hint, STYLE_TEXT_DIM);
    if (p->pack_count == 0)
        render_text(cells, cols, rows, LEFT, TOP, "(your pack is empty)", STYLE_TEXT_DIM);

    int h = entry_rows(ui, p->pack_count, rows - TOP - 2);
    for (int i = 0; i < p->pack_count; i++)
    {
        char name[80], letter[4] = {(char)('a' + i), ' ', '-', '\0'};
        item_name(&g->lore, &p->pack[i], name, sizeof name);
        int y = TOP + i * h;
        int x = render_text(cells, cols, rows, LEFT, y, letter, STYLE_KEY);
        render_text(cells, cols, rows, x + 1, y, name, STYLE_TEXT);
        add_target(ui, 0, y, cols, h, 'a' + i);
    }
    char gold[32];
    snprintf(gold, sizeof gold, "Gold: %d", p->gold);
    render_text(cells, cols, rows, LEFT, TOP + (p->pack_count > 0 ? p->pack_count * h : 1) + 1,
                gold, STYLE_GOLD);
}

static void render_options(Ui *ui, Cell *cells, int cols, int rows)
{
    enum
    {
        LEFT = 2,
        TOP = 4,
        VALUE_X = 34,
    };
    static const char *const SPEEDS[SPEED_COUNT] = {"slow", "normal", "fast"};
    static const char *const COLOURS[COLOURS_COUNT] = {
        "auto (what the terminal reports)", "full (256 colours)", "8 colours", "monochrome"};
    const Settings *s = ui->settings;
    char speed[48], lines[8];
    snprintf(speed, sizeof speed, "%s: a turn every %d ms", SPEEDS[s->speed], settings_tick_ms(s));
    snprintf(lines, sizeof lines, "%d", s->log_lines);
    const char *const rows_text[][2] = {
        {"Start new games in real time", s->realtime_start ? "on" : "off"},
        {"Real-time speed", speed},
        {"Message lines above the map", lines},
        {"Colours", COLOURS[s->colours]},
    };

    render_clear(cells, cols, rows, STYLE_DEFAULT);
    render_text(cells, cols, rows, LEFT, 0, "Options", STYLE_HEADING);
    render_text(cells, cols, rows, LEFT, 1,
                ui->touch ? "Tap an option to change it."
                          : "Up/Down choose, Left/Right/Enter change; any other key goes back.",
                STYLE_TEXT_DIM);
    render_text(cells, cols, rows, LEFT, 2,
                ui->touch ? "Kept for later games."
                          : "Kept for later games. T switches real time in a game.",
                STYLE_TEXT_DIM);

    /* Narrow screens: each value under its name. */
    bool stacked = cols < VALUE_X + 16;
    int text_rows = stacked ? 2 : 1;
    int h = text_rows + (entry_rows(ui, option_count(ui), (rows - TOP) / text_rows) - 1);
    for (int i = 0; i < option_count(ui); i++)
    {
        bool chosen = i == ui->option_row;
        int y = TOP + i * h;
        render_text(cells, cols, rows, LEFT, y, chosen ? ">" : " ", STYLE_KEY);
        render_text(cells, cols, rows, LEFT + 2, y, rows_text[i][0],
                    chosen ? STYLE_HEADING : STYLE_TEXT);
        render_text(cells, cols, rows, stacked ? LEFT + 4 : VALUE_X, stacked ? y + 1 : y,
                    rows_text[i][1], STYLE_KEY);
        add_target(ui, 0, y, cols, h, UI_KEY_ROW + i);
    }
}

/* The touch menu: one button per entry. */
static void render_menu(Ui *ui, const Game *g, Cell *cells, int cols, int rows)
{
    enum
    {
        LEFT = 2,
        TOP = 2,
        WIDTH_MAX = 36,
    };
    MenuEntry e[6];
    int n = menu_entries(ui, g, e);
    render_clear(cells, cols, rows, STYLE_DEFAULT);
    render_text(cells, cols, rows, LEFT, 0, "Menu", STYLE_HEADING);

    int h = (rows - TOP) / n; /* rows per entry: a button and a gap */
    if (h > 4)
        h = 4;
    int w = cols - 2 * LEFT < WIDTH_MAX ? cols - 2 * LEFT : WIDTH_MAX;
    for (int i = 0; h > 0 && i < n; i++)
        render_button(ui, cells, cols, rows, LEFT, TOP + i * h, w, h > 1 ? h - 1 : 1,
                      &(Button){.label = e[i].label, .key = e[i].key});
}

/* The map screen; touch: tapping the messages opens the history. */
static void render_map_screen(Ui *ui, const Game *g, Cell *cells, int cols, int rows)
{
    MapView view = {.log_lines = ui->settings ? ui->settings->log_lines : LOG_LINES_MIN,
                    .can_exit = ui->can_exit,
                    .no_hints = ui->touch};
    render_game(g, cells, cols, rows, view);
    ui->map_layout = render_map_layout(g, cols, rows, view);
    add_target(ui, 0, 0, cols, ui->map_layout.log_rows, 'm');
    if (ui->confirm_abandon)
    {
        const char *question = ui->touch
                                   ? "Give up this game? It won't be saved or scored."
                                   : "Give up this game? It won't be saved or scored. (y / n)";
        int start[4], len[4];
        int lines = render_wrap(question, cols - 1, start, len, 4);
        render_clear(cells, cols, lines < rows ? lines : rows, STYLE_DEFAULT);
        render_wrapped(cells, cols, rows, 1, 0, question, STYLE_HEADING);
        ui->target_count = 0; /* only the answers */
    }
}

void ui_render(Ui *ui, const Game *g, Cell *cells, int cols, int rows)
{
    ui->target_count = 0;
    if (cols <= 0 || rows <= 0)
        return;

    int bar = bar_rows(ui, rows);
    int screen_rows = rows - bar;
    Button back = {.label = "Back", .key = 27};
    switch (ui->screen)
    {
    case SCREEN_MAP:
        render_map_screen(ui, g, cells, cols, screen_rows);
        if (ui->confirm_abandon)
            render_bar(ui, cells, cols, rows, bar,
                       (Button[]){{"Give up", NULL, 'y'}, {"Keep playing", "Keep", 'n'}}, 2);
        else
            render_bar(ui, cells, cols, rows, bar,
                       (Button[]){{"Items", "Inv", 'i'},
                                  {"Drop", NULL, 'x'},
                                  {"Stairs", "><", UI_KEY_STAIRS},
                                  {"Rest", NULL, 'Z'},
                                  {"Menu", NULL, UI_KEY_MENU}},
                       5);
        break;
    case SCREEN_HELP:
        render_help(ui, g, cells, cols, screen_rows);
        render_bar(ui, cells, cols, rows, bar, &back, 1);
        break;
    case SCREEN_LOG:
        render_history(ui, g, cells, cols, screen_rows);
        render_bar(
            ui, cells, cols, rows, bar,
            (Button[]){{"Older", NULL, UI_KEY_PAGE_UP}, {"Newer", NULL, UI_KEY_PAGE_DOWN}, back},
            3);
        break;
    case SCREEN_END:
        render_end(ui, g, cells, cols, screen_rows);
        if (ui->can_exit)
            render_bar(ui, cells, cols, rows, bar,
                       (Button[]){{"New game", "New", '\n'}, {"Quit", NULL, 'q'}}, 2);
        else
            render_bar(ui, cells, cols, rows, bar, &(Button){"New game", NULL, '\n'}, 1);
        break;
    case SCREEN_PACK:
        render_pack(ui, g, cells, cols, screen_rows);
        render_bar(ui, cells, cols, rows, bar, &back, 1);
        break;
    case SCREEN_OPTIONS:
        render_options(ui, cells, cols, screen_rows);
        render_bar(ui, cells, cols, rows, bar, &back, 1);
        break;
    case SCREEN_MENU:
        render_menu(ui, g, cells, cols, screen_rows);
        render_bar(ui, cells, cols, rows, bar, &back, 1);
        break;
    }
}
