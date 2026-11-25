#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "core/render.h"

static Cell cells[50 * 20];

static int find_player(int cols, int rows, int *px, int *py)
{
    int found = 0;
    for (int y = 0; y < rows; y++)
        for (int x = 0; x < cols; x++)
            if (cells[y * cols + x].glyph == '@')
            {
                *px = x;
                *py = y;
                found++;
            }
    return found;
}

static void player_shown_once_with_status_line(void)
{
    Game g;
    game_init(&g, 5);
    int cols = 50, rows = 20, px = -1, py = -1;
    render_game(&g, cells, cols, rows);

    CHECK(find_player(cols, rows, &px, &py) == 1);
    CHECK(cells[py * cols + px].style == STYLE_PLAYER);
    CHECK(py < rows - 1);
    CHECK(cells[(rows - 1) * cols + 1].glyph == 'S'); /* " Seed ..." */
    CHECK(cells[(rows - 1) * cols].style == STYLE_STATUS);
    game_free(&g);
}

static void unexplored_cells_are_blank_and_seen_cells_dim(void)
{
    Game g;
    game_init(&g, 5);
    map_clear_visible(&g.map);
    map_clear_explored(&g.map);
    render_game(&g, cells, 50, 20);
    for (int i = 2 * 50; i < 50 * 19; i++) /* below the two message lines */
        CHECK(cells[i].glyph == ' ' || cells[i].glyph == '@');

    /* Mark the cell east of the player explored but not visible. */
    int px = -1, py = -1;
    find_player(50, 20, &px, &py);
    map_mark_seen(&g.map, g.player.pos.x + 1, g.player.pos.y);
    map_clear_visible(&g.map);
    render_game(&g, cells, 50, 20);
    uint8_t s = cells[py * 50 + px + 1].style;
    CHECK(s == STYLE_FLOOR_SEEN || s == STYLE_WALL_SEEN);
    game_free(&g);
}

static void tiny_and_huge_screens_are_safe(void)
{
    Game g;
    game_init(&g, 5);
    render_game(&g, cells, 1, 1);
    CHECK(cells[0].glyph == '@'); /* camera centres on the player */
    render_game(&g, cells, 0, 0);
    game_free(&g);

    static Cell big[200 * 60];
    game_init(&g, 5);
    render_game(&g, big, 200, 60); /* larger than the map: centred */
    int at = 0;
    for (int i = 0; i < 200 * 60; i++)
        at += big[i].glyph == '@';
    CHECK(at == 1);
    game_free(&g);
}

/* Two message lines on top from 10 rows up, oldest first; fresh ones bright. */
static void messages_on_top(void)
{
    Game g;
    game_init(&g, 5);
    msglog_add(&g.log, 0, "Second");
    render_game(&g, cells, 50, 20);
    CHECK(cells[1].glyph == 'Y' && cells[1].style == STYLE_TEXT); /* "You enter..." */
    CHECK(cells[50 + 1].glyph == 'S' && cells[50 + 1].style == STYLE_TEXT);

    game_step(&g, ACTION_WAIT);
    render_game(&g, cells, 50, 20);
    CHECK(cells[50 + 1].style == STYLE_TEXT_DIM);

    render_game(&g, cells, 50, 9); /* too small: map and status only */
    int px = -1, py = -1;
    CHECK(find_player(50, 9, &px, &py) == 1);
    CHECK(cells[1].glyph != 'Y');
    game_free(&g);
}

/* Every style is readable: its text colour differs from its background. */
static void palette_has_contrast(void)
{
    for (int s = 0; s < STYLE_COUNT; s++)
    {
        const StyleColor *c = &STYLE_COLORS[s];
        Rgb bg = c->has_bg ? c->bg : PALETTE_BACKGROUND;
        int diff = abs(c->fg.r - bg.r) + abs(c->fg.g - bg.g) + abs(c->fg.b - bg.b);
        CHECK(diff >= 40);
    }
}

static void utf8_encoding(void)
{
    char s[5];
    CHECK(utf8_encode('@', s) == 1 && strcmp(s, "@") == 0);
    CHECK(utf8_encode(0x2588, s) == 3 && strcmp(s, "\xE2\x96\x88") == 0);
    CHECK(utf8_encode(0xE9, s) == 2 && strcmp(s, "\xC3\xA9") == 0);
}

void run_render_tests(void)
{
    RUN(player_shown_once_with_status_line);
    RUN(unexplored_cells_are_blank_and_seen_cells_dim);
    RUN(tiny_and_huge_screens_are_safe);
    RUN(messages_on_top);
    RUN(palette_has_contrast);
    RUN(utf8_encoding);
}
