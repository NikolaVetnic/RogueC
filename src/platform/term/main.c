/* Terminal frontend: ncurses input and drawing around the core. */
#include <locale.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "core/game.h"
#include "core/render.h"

/* ncurses attributes for each Style, filled by init_styles. */
static attr_t style_attr[STYLE_COUNT];

static void init_styles(void)
{
    style_attr[STYLE_PLAYER] = A_BOLD;
    style_attr[STYLE_FLOOR_SEEN] = A_DIM;
    style_attr[STYLE_WALL_SEEN] = A_DIM;
    style_attr[STYLE_STATUS] = A_REVERSE;

    if (!has_colors())
        return;

    start_color();
    use_default_colors();
    short seen = COLORS >= 16 ? 8 : COLOR_BLUE; /* grey if available */

    init_pair(1, COLOR_WHITE, -1);
    init_pair(2, seen, -1);
    init_pair(3, COLOR_YELLOW, -1);

    style_attr[STYLE_FLOOR_LIT] = COLOR_PAIR(1);
    style_attr[STYLE_WALL_LIT] = COLOR_PAIR(1);
    style_attr[STYLE_FLOOR_SEEN] = COLOR_PAIR(2);
    style_attr[STYLE_WALL_SEEN] = COLOR_PAIR(2);
    style_attr[STYLE_PLAYER] = COLOR_PAIR(3) | A_BOLD;
}

static Action key_to_action(int key)
{
    // clang-format off
    switch (key)
    {
    case 'w': case 'W': case 'k': case '8': case KEY_UP:    return ACTION_MOVE_N;
    case 's': case 'S': case 'j': case '2': case KEY_DOWN:  return ACTION_MOVE_S;
    case 'a': case 'A': case 'h': case '4': case KEY_LEFT:  return ACTION_MOVE_W;
    case 'd': case 'D': case 'l': case '6': case KEY_RIGHT: return ACTION_MOVE_E;
    case 'y': case '7': case KEY_HOME:  return ACTION_MOVE_NW;
    case 'u': case '9': case KEY_PPAGE: return ACTION_MOVE_NE;
    case 'b': case '1': case KEY_END:   return ACTION_MOVE_SW;
    case 'n': case '3': case KEY_NPAGE: return ACTION_MOVE_SE;
    case '.': case '5': case KEY_B2:    return ACTION_WAIT;
    case 'q': case 'Q':                 return ACTION_QUIT;
    default:                            return ACTION_NONE;
    }
    // clang-format on
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

    setlocale(LC_ALL, ""); /* UTF-8 output for the wall glyph */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    init_styles();

    Cell *cells = NULL;
    int cols = 0, rows = 0;

    while (game.running)
    {
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

        render_game(&game, cells, cols, rows);
        draw(cells, cols, rows);
        game_step(&game, key_to_action(getch()));
    }

    endwin();
    free(cells);
    game_free(&game);
    printf("Seed %llu, %d turns.\n", (unsigned long long)seed, game.turn);
    return EXIT_SUCCESS;
}
