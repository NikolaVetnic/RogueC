/* Terminal frontend: ncurses input and drawing around the core. */
#define _POSIX_C_SOURCE 200809L /* clock_gettime */

#include <locale.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "core/game.h"
#include "core/render.h"
#include "core/ui.h"

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

static void init_styles(void)
{
    if (!has_colors()) /* monochrome: brightness only */
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
    if (COLORS < 256 || COLOR_PAIRS <= STYLE_COUNT)
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

int main(int argc, char **argv)
{
    uint64_t seed = argc > 1 ? strtoull(argv[1], NULL, 10) : (uint64_t)time(NULL);

    Game game;
    if (!game_init(&game, seed))
    {
        fprintf(stderr, "Out of memory while generating the level.\n");
        return EXIT_FAILURE;
    }
    Ui ui;
    ui_init(&ui);

    setlocale(LC_ALL, ""); /* UTF-8 output for the wall glyph */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    set_escdelay(25); /* Esc alone arrives after this many ms, not 1 s */
    curs_set(0);
    init_styles();

    Cell *cells = NULL;
    int cols = 0, rows = 0;
    long last = now_ms();

    while (game.running)
    {
        if (ui.new_game) /* from the death screen */
        {
            game_free(&game);
            seed = (uint64_t)time(NULL);
            if (!game_init(&game, seed))
                break;
            ui_init(&ui);
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
        timeout(game.realtime ? 20 : -1); /* real time: wake up for the clock */
        int key = ui_key_for(getch());
        long now = now_ms();
        ui_update(&ui, &game, (int)(now - last));
        last = now;
        if (key != 0)
            ui_key(&ui, &game, key);
    }

    endwin();
    free(cells);
    game_free(&game);
    printf("Seed %llu, %d turns.\n", (unsigned long long)seed, game.turn);
    return EXIT_SUCCESS;
}
