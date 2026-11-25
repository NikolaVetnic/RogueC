/* Terminal frontend: ncurses input and drawing around the core. */
#define _POSIX_C_SOURCE 200809L /* clock_gettime, sigaction */

#include <locale.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/game.h"
#include "core/render.h"
#include "core/ui.h"
#include "platform/common/store.h"

/* Set by SIGINT, SIGTERM or SIGHUP (terminal closed): save and leave. */
static volatile sig_atomic_t stop_signal = 0;

static void on_stop_signal(int sig)
{
    (void)sig;
    stop_signal = 1;
}

/* Without SA_RESTART, so a waiting getch returns at once. */
static void catch_stop_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_stop_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
}

/* ncurses attributes for each Style, filled by init_styles. */
static attr_t style_attr[STYLE_COUNT];

/* Nearest colour of the xterm 256-colour palette: the 6x6x6 cube (16-231) or
 * the grey ramp (232-255). */
static short xterm_color(Rgb c)
{
    static const int LEVEL[6] = {0, 95, 135, 175, 215, 255};
    int idx[3], v[3] = {c.r, c.g, c.b};
    long cube_dist = 0;
    for (int i = 0; i < 3; i++)
    {
        idx[i] = 0;
        for (int l = 1; l < 6; l++)
            if (abs(LEVEL[l] - v[i]) < abs(LEVEL[idx[i]] - v[i]))
                idx[i] = l;
        long d = LEVEL[idx[i]] - v[i];
        cube_dist += d * d;
    }

    int grey = ((c.r + c.g + c.b) / 3 - 8 + 5) / 10; /* ramp: 8, 18, ..., 238 */
    if (grey < 0)
        grey = 0;
    if (grey > 23)
        grey = 23;
    long grey_dist = 0;
    for (int i = 0; i < 3; i++)
    {
        long d = 8 + 10 * grey - v[i];
        grey_dist += d * d;
    }

    if (grey_dist < cube_dist)
        return (short)(232 + grey);
    return (short)(16 + 36 * idx[0] + 6 * idx[1] + idx[2]);
}

/* Fewer than 256 colours: a rough version of the palette. */
static void init_styles_8(void)
{
    static const struct
    {
        short fg;
        attr_t attr;
    } BASIC[STYLE_COUNT] = {
        [STYLE_DEFAULT] = {COLOR_WHITE, 0},
        [STYLE_FLOOR_LIT] = {COLOR_YELLOW, 0},
        [STYLE_FLOOR_SEEN] = {COLOR_BLUE, 0},
        [STYLE_WALL_LIT] = {COLOR_YELLOW, 0},
        [STYLE_WALL_SEEN] = {COLOR_BLUE, 0},
        [STYLE_STAIRS_LIT] = {COLOR_CYAN, A_BOLD},
        [STYLE_STAIRS_SEEN] = {COLOR_CYAN, 0},
        [STYLE_PLAYER] = {COLOR_YELLOW, A_BOLD},
        [STYLE_MON_BEAST] = {COLOR_YELLOW, 0},
        [STYLE_MON_HUMANOID] = {COLOR_GREEN, A_BOLD},
        [STYLE_MON_UNDEAD] = {COLOR_WHITE, A_BOLD},
        [STYLE_MON_BRUTE] = {COLOR_RED, A_BOLD},
        [STYLE_ITEM] = {COLOR_MAGENTA, A_BOLD},
        [STYLE_ITEM_SEEN] = {COLOR_MAGENTA, 0},
        [STYLE_GOLD] = {COLOR_YELLOW, A_BOLD},
        [STYLE_STATUS_ALERT] = {COLOR_RED, A_REVERSE | A_BOLD},
        [STYLE_STATUS] = {COLOR_WHITE, A_REVERSE},
        [STYLE_TEXT] = {COLOR_WHITE, A_BOLD},
        [STYLE_TEXT_DIM] = {COLOR_WHITE, A_DIM},
        [STYLE_HEADING] = {COLOR_YELLOW, A_BOLD},
        [STYLE_KEY] = {COLOR_CYAN, 0},
    };
    for (int s = 0; s < STYLE_COUNT; s++)
    {
        init_pair((short)(s + 1), BASIC[s].fg, -1);
        style_attr[s] = COLOR_PAIR(s + 1) | BASIC[s].attr;
    }
}

/* The colour mode from the options; COLOURS_AUTO and COLOURS_FULL use 256
 * colours if the terminal has them. */
static void init_styles(Colours mode)
{
    memset(style_attr, 0, sizeof style_attr);
    if (!has_colors() || mode == COLOURS_MONO) /* monochrome: brightness only */
    {
        style_attr[STYLE_PLAYER] = A_BOLD;
        style_attr[STYLE_FLOOR_SEEN] = A_DIM;
        style_attr[STYLE_WALL_SEEN] = A_DIM;
        style_attr[STYLE_STAIRS_LIT] = A_BOLD;
        style_attr[STYLE_STAIRS_SEEN] = A_DIM;
        style_attr[STYLE_STATUS] = A_REVERSE;
        style_attr[STYLE_STATUS_ALERT] = A_REVERSE | A_BOLD;
        style_attr[STYLE_ITEM_SEEN] = A_DIM;
        style_attr[STYLE_TEXT_DIM] = A_DIM;
        style_attr[STYLE_HEADING] = A_BOLD;
        for (int s = STYLE_MON_BEAST; s <= STYLE_MON_BRUTE; s++)
            style_attr[s] = A_BOLD;
        return;
    }

    start_color();
    use_default_colors(); /* -1: the terminal's own background */
    if (mode == COLOURS_8 || COLORS < 256 || COLOR_PAIRS <= STYLE_COUNT)
    {
        init_styles_8();
        return;
    }

    for (int s = 0; s < STYLE_COUNT; s++)
    {
        const StyleColor *c = &STYLE_COLORS[s];
        init_pair((short)(s + 1), xterm_color(c->fg), c->has_bg ? xterm_color(c->bg) : -1);
        style_attr[s] = COLOR_PAIR(s + 1);
    }
    style_attr[STYLE_PLAYER] |= A_BOLD;
    style_attr[STYLE_HEADING] |= A_BOLD;
}

/* ncurses key -> ui key; 0 for no key press. */
static int ui_key_for(int key)
{
    // clang-format off
    switch (key)
    {
    case ERR: case KEY_RESIZE:        return 0;
    case KEY_UP:                      return UI_KEY_UP;
    case KEY_DOWN:                    return UI_KEY_DOWN;
    case KEY_LEFT:                    return UI_KEY_LEFT;
    case KEY_RIGHT:                   return UI_KEY_RIGHT;
    case KEY_HOME: case KEY_A1:       return UI_KEY_HOME;
    case KEY_END: case KEY_C1:        return UI_KEY_END;
    case KEY_PPAGE: case KEY_A3:      return UI_KEY_PAGE_UP;
    case KEY_NPAGE: case KEY_C3:      return UI_KEY_PAGE_DOWN;
    case KEY_B2:                      return UI_KEY_CENTER;
    case KEY_SR:                      return UI_KEY_UP | UI_KEY_SHIFT; /* Shift: run */
    case KEY_SF:                      return UI_KEY_DOWN | UI_KEY_SHIFT;
    case KEY_SLEFT:                   return UI_KEY_LEFT | UI_KEY_SHIFT;
    case KEY_SRIGHT:                  return UI_KEY_RIGHT | UI_KEY_SHIFT;
    case KEY_SHOME:                   return UI_KEY_HOME | UI_KEY_SHIFT;
    case KEY_SEND:                    return UI_KEY_END | UI_KEY_SHIFT;
    case KEY_SPREVIOUS:               return UI_KEY_PAGE_UP | UI_KEY_SHIFT;
    case KEY_SNEXT:                   return UI_KEY_PAGE_DOWN | UI_KEY_SHIFT;
    case KEY_ENTER:                   return '\n';
    default:                          return key >= KEY_MIN ? UI_KEY_OTHER : key;
    }
    // clang-format on
}

static long now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static void draw(const Cell *cells, int cols, int rows)
{
    char utf8[5];
    for (int y = 0; y < rows; y++)
    {
        move(y, 0);
        for (int x = 0; x < cols; x++)
        {
            const Cell *c = &cells[y * cols + x];
            attrset(style_attr[c->style]);
            utf8_encode(c->glyph, utf8);
            addstr(utf8); /* the bottom-right cell returns ERR but is drawn */
        }
    }
    attrset(A_NORMAL);
    refresh();
}

static void new_ui(Ui *ui, ScoreTable *scores, Settings *settings)
{
    ui_init(ui, scores, settings);
    ui->can_exit = true;
    ui->colour_option = true;
}

/* The frontend's side of the ui's requests: saving and deleting. */
static void store_requests(Ui *ui, const Game *g, const ScoreTable *scores,
                           const Settings *settings)
{
    if (ui->scores_changed)
        store_save_scores(scores);
    if (ui->settings_changed)
        store_save_settings(settings);
    if (ui->save_wanted)
        store_save_game(g);
    if (ui->save_delete)
        store_delete_game();
    ui->scores_changed = ui->settings_changed = ui->save_wanted = ui->save_delete = false;
}

/* `rogue` resumes the saved game or starts a new one; `rogue SEED` starts a
 * new game from SEED (replacing the save when it next saves). */
int main(int argc, char **argv)
{
    bool resume = argc < 2;
    uint64_t seed = resume ? (uint64_t)time(NULL) : strtoull(argv[1], NULL, 10);

    static ScoreTable scores;
    store_load_scores(&scores);
    Settings settings = store_load_settings();
    Game game;
    if (!store_start_game(&game, resume, seed, &settings))
    {
        fprintf(stderr, "Out of memory while generating the level.\n");
        return EXIT_FAILURE;
    }
    Ui ui;
    new_ui(&ui, &scores, &settings);

    catch_stop_signals();
    setlocale(LC_ALL, ""); /* UTF-8 output for the wall glyph */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    set_escdelay(25); /* Esc alone arrives after this many ms, not 1 s */
    curs_set(0);
    Colours colours = settings.colours;
    init_styles(colours);

    Cell *cells = NULL;
    int cols = 0, rows = 0;
    long last = now_ms();

    while (game.running && !stop_signal)
    {
        if (ui.new_game) /* from the end screen */
        {
            game_free(&game);
            if (!store_start_game(&game, false, (uint64_t)time(NULL), &settings))
                break;
            new_ui(&ui, &scores, &settings);
        }
        if (settings.colours != colours)
        {
            colours = settings.colours;
            init_styles(colours);
            clear();
        }

        if (cols != COLS || rows != LINES) /* first frame or terminal resized */
        {
            Cell *resized = realloc(cells, (size_t)COLS * (size_t)LINES * sizeof *cells);
            if (!resized)
                break;
            cells = resized;
            cols = COLS;
            rows = LINES;
            clear();
        }

        ui_render(&ui, &game, cells, cols, rows);
        draw(cells, cols, rows);
        timeout(game.realtime ? 20 : 500); /* real time: wake up for the clock */
        int key = ui_key_for(getch());
        long now = now_ms();
        ui_update(&ui, &game, (int)(now - last));
        last = now;
        if (key != 0 && !stop_signal)
            ui_key(&ui, &game, key);
        store_requests(&ui, &game, &scores, &settings);
    }

    if (stop_signal && !ui.save_delete)
        store_save_game(&game); /* does nothing if the game is over */
    endwin();
    free(cells);
    seed = game.seed;
    int turns = game.turn;
    bool over = game_over(&game);
    game_free(&game);
    if (over)
        printf("Seed %llu, %d turns.\n", (unsigned long long)seed, turns);
    else
        printf("Game saved: run rogue again to go on.\n");
    return EXIT_SUCCESS;
}
