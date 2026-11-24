#include "core/render.h"

#include <inttypes.h>
#include <stdio.h>

#define GLYPH_WALL 0x2588u /* █ */
#define GLYPH_FLOOR '.'
#define GLYPH_PLAYER '@'

static void put(Cell *cells, int cols, int x, int y, uint32_t glyph, Style style)
{
    cells[y * cols + x] = (Cell){.glyph = glyph, .style = (uint8_t)style};
}

/* Left/top map coordinate shown at screen column/row 0 on one axis: centre the
 * map if it fits, else follow the player without scrolling past the edges. */
static int camera_origin(int map_size, int view_size, int player)
{
    if (map_size <= view_size)
        return -(view_size - map_size) / 2;

    int origin = player - view_size / 2;
    if (origin < 0)
        origin = 0;
    if (origin > map_size - view_size)
        origin = map_size - view_size;
    return origin;
}

static void render_cell(const Game *g, Cell *cells, int cols, int sx, int sy, int mx, int my)
{
    const Map *m = &g->map;

    if (vec2_eq(g->player.pos, vec2(mx, my)))
        put(cells, cols, sx, sy, GLYPH_PLAYER, STYLE_PLAYER);
    else if (!map_is_explored(m, mx, my))
        put(cells, cols, sx, sy, ' ', STYLE_DEFAULT);
    else
    {
        bool lit = map_is_visible(m, mx, my);
        if (map_get(m, mx, my) == TILE_WALL)
            put(cells, cols, sx, sy, GLYPH_WALL, lit ? STYLE_WALL_LIT : STYLE_WALL_SEEN);
        else
            put(cells, cols, sx, sy, GLYPH_FLOOR, lit ? STYLE_FLOOR_LIT : STYLE_FLOOR_SEEN);
    }
}

static void render_status(const Game *g, Cell *cells, int cols, int row)
{
    char text[128];
    snprintf(text, sizeof text, " Seed %" PRIu64 "   Turn %d   HP %d/%d   q: quit", g->seed,
             g->turn, g->player.hp, g->player.hp_max);

    int x = 0;
    for (; x < cols && text[x] != '\0'; x++)
        put(cells, cols, x, row, (unsigned char)text[x], STYLE_STATUS);
    for (; x < cols; x++)
        put(cells, cols, x, row, ' ', STYLE_STATUS);
}

void render_game(const Game *g, Cell *cells, int cols, int rows)
{
    if (cols <= 0 || rows <= 0)
        return;

    int view_rows = rows >= 2 ? rows - 1 : rows;
    int cam_x = camera_origin(g->map.w, cols, g->player.pos.x);
    int cam_y = camera_origin(g->map.h, view_rows, g->player.pos.y);

    for (int sy = 0; sy < view_rows; sy++)
        for (int sx = 0; sx < cols; sx++)
            render_cell(g, cells, cols, sx, sy, cam_x + sx, cam_y + sy);

    if (view_rows < rows)
        render_status(g, cells, cols, rows - 1);
}

int utf8_encode(uint32_t cp, char out[5])
{
    int n;
    if (cp < 0x80)
    {
        out[0] = (char)cp;
        n = 1;
    }
    else if (cp < 0x800)
    {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        n = 2;
    }
    else if (cp < 0x10000)
    {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        n = 3;
    }
    else
    {
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        n = 4;
    }
    out[n] = '\0';
    return n;
}
