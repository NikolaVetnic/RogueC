#include "core/render.h"

#include <stdio.h>

enum
{
    LOG_LINES = 2,     /* messages shown on the map screen */
    LOG_MIN_ROWS = 10, /* below this, the map gets the room */
};

/* Warm light in view, cool blue memory out of view. */
const Rgb PALETTE_BACKGROUND = {12, 12, 16};

const StyleColor STYLE_COLORS[STYLE_COUNT] = {
    [STYLE_DEFAULT] = {.fg = {200, 200, 200}},
    [STYLE_FLOOR_LIT] = {.fg = {190, 172, 136}},
    [STYLE_FLOOR_SEEN] = {.fg = {54, 62, 86}},
    [STYLE_WALL_LIT] = {.fg = {122, 104, 84}},
    [STYLE_WALL_SEEN] = {.fg = {36, 41, 58}},
    [STYLE_STAIRS_LIT] = {.fg = {110, 222, 206}},
    [STYLE_STAIRS_SEEN] = {.fg = {62, 108, 118}},
    [STYLE_PLAYER] = {.fg = {255, 210, 70}},
    [STYLE_MON_BEAST] = {.fg = {214, 150, 96}},
    [STYLE_MON_HUMANOID] = {.fg = {132, 204, 96}},
    [STYLE_MON_UNDEAD] = {.fg = {196, 206, 232}},
    [STYLE_MON_BRUTE] = {.fg = {236, 86, 74}},
    [STYLE_ITEM] = {.fg = {226, 140, 226}},
    [STYLE_ITEM_SEEN] = {.fg = {110, 76, 120}},
    [STYLE_GOLD] = {.fg = {250, 226, 90}},
    [STYLE_STATUS] = {.fg = {222, 212, 192}, .bg = {50, 45, 40}, .has_bg = true},
    [STYLE_STATUS_ALERT] = {.fg = {255, 120, 100}, .bg = {50, 45, 40}, .has_bg = true},
    [STYLE_TEXT] = {.fg = {226, 220, 208}},
    [STYLE_TEXT_DIM] = {.fg = {112, 114, 128}},
    [STYLE_HEADING] = {.fg = {255, 210, 70}},
    [STYLE_KEY] = {.fg = {130, 186, 230}},
};

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

Style render_monster_style(MonsterKind kind)
{
    switch (MONSTERS[kind].look)
    {
    case LOOK_HUMANOID:
        return STYLE_MON_HUMANOID;
    case LOOK_UNDEAD:
        return STYLE_MON_UNDEAD;
    case LOOK_BRUTE:
        return STYLE_MON_BRUTE;
    default:
        return STYLE_MON_BEAST;
    }
}

static void render_cell(const Game *g, Cell *cells, int cols, int sx, int sy, int mx, int my)
{
    const Map *m = g->map;
    const Level *lv = &g->levels[g->depth - 1];
    int mon = map_is_visible(m, mx, my) ? level_monster_at(lv, vec2(mx, my)) : -1;
    int item = level_item_at(lv, vec2(mx, my));
    if (item >= 0 && (!lv->items[item].seen || !map_is_explored(m, mx, my)))
        item = -1;

    if (vec2_eq(g->player.pos, vec2(mx, my)))
        put(cells, cols, sx, sy, GLYPH_PLAYER, STYLE_PLAYER);
    else if (mon >= 0)
        put(cells, cols, sx, sy, MONSTERS[lv->monsters[mon].kind].glyph,
            render_monster_style(lv->monsters[mon].kind));
    else if (item >= 0)
    {
        ItemClass cls = lv->items[item].item.cls;
        Style s = !map_is_visible(m, mx, my) ? STYLE_ITEM_SEEN
                  : cls == ITEM_GOLD         ? STYLE_GOLD
                                             : STYLE_ITEM;
        put(cells, cols, sx, sy, item_glyph(cls), s);
    }
    else if (!map_is_explored(m, mx, my))
        put(cells, cols, sx, sy, ' ', STYLE_DEFAULT);
    else
    {
        bool lit = map_is_visible(m, mx, my);
        switch (map_get(m, mx, my))
        {
        case TILE_WALL:
            put(cells, cols, sx, sy, GLYPH_WALL, lit ? STYLE_WALL_LIT : STYLE_WALL_SEEN);
            break;
        case TILE_STAIRS_DOWN:
        case TILE_STAIRS_UP:
            put(cells, cols, sx, sy,
                map_get(m, mx, my) == TILE_STAIRS_DOWN ? GLYPH_STAIRS_DOWN : GLYPH_STAIRS_UP,
                lit ? STYLE_STAIRS_LIT : STYLE_STAIRS_SEEN);
            break;
        default:
            put(cells, cols, sx, sy, GLYPH_FLOOR, lit ? STYLE_FLOOR_LIT : STYLE_FLOOR_SEEN);
            break;
        }
    }
}

void render_clear(Cell *cells, int cols, int rows, Style style)
{
    for (int i = 0; i < cols * rows; i++)
        cells[i] = (Cell){.glyph = ' ', .style = (uint8_t)style};
}

int render_text(Cell *cells, int cols, int rows, int x, int y, const char *text, Style style)
{
    if (y < 0 || y >= rows)
        return x;
    for (; *text != '\0'; text++, x++)
        if (x >= 0 && x < cols)
            put(cells, cols, x, y, (unsigned char)*text, style);
    return x;
}

void render_message_text(const Message *m, char *out, int size)
{
    if (m->count > 1)
        snprintf(out, (size_t)size, "%s (x%d)", m->text, m->count);
    else
        snprintf(out, (size_t)size, "%s", m->text);
}

/* " Depth 3   HP 12/20   Gold 45   Turn 345   Hungry Hasted   REAL TIME   ? help  i items",
 * the warnings in the alert style. */
static void render_status(const Game *g, Cell *cells, int cols, int row)
{
    static const char *const HUNGER[] = {"", "Hungry", "Weak", "Starving"};
    const Player *p = &g->player;
    char text[96];
    snprintf(text, sizeof text, " Depth %d   HP %d/%d   Gold %d   Turn %d  ", g->depth, p->hp,
             p->base.hp_max, p->gold, g->turn);

    render_clear(cells + row * cols, cols, 1, STYLE_STATUS);
    int x = render_text(cells, cols, row + 1, 0, row, text, STYLE_STATUS);
    const char *alerts[] = {HUNGER[game_hunger(g)], p->haste > 0 ? "Fast" : "",
                            p->confused > 0 ? "Confused" : "", p->blind > 0 ? "Blind" : ""};
    for (size_t i = 0; i < sizeof alerts / sizeof alerts[0]; i++)
        if (alerts[i][0] != '\0')
            x = render_text(cells, cols, row + 1, x + 1, row, alerts[i], STYLE_STATUS_ALERT);
    if (g->realtime)
        x = render_text(cells, cols, row + 1, x + 3, row, "REAL TIME", STYLE_STATUS);
    render_text(cells, cols, row + 1, x + 6, row, "? help   i items   m messages   q quit",
                STYLE_STATUS);
}

/* The newest messages, oldest of them on top; bright if they came after the last action. */
static void render_log(const Game *g, Cell *cells, int cols, int lines)
{
    render_clear(cells, cols, lines, STYLE_DEFAULT);
    for (int line = 0; line < lines; line++)
    {
        const Message *m = msglog_get(&g->log, lines - 1 - line);
        if (!m)
            continue;
        char text[MSG_TEXT_SIZE + 16];
        render_message_text(m, text, sizeof text);
        render_text(cells, cols, lines, 1, line, text,
                    m->serial >= g->fresh_serial ? STYLE_TEXT : STYLE_TEXT_DIM);
    }
}

void render_game(const Game *g, Cell *cells, int cols, int rows)
{
    if (cols <= 0 || rows <= 0)
        return;

    int log_rows = rows >= LOG_MIN_ROWS ? LOG_LINES : 0;
    int status_rows = rows >= 2 ? 1 : 0;
    int view_rows = rows - log_rows - status_rows;
    int cam_x = camera_origin(g->map->w, cols, g->player.pos.x);
    int cam_y = camera_origin(g->map->h, view_rows, g->player.pos.y);

    render_log(g, cells, cols, log_rows);
    Cell *view = cells + log_rows * cols;
    for (int sy = 0; sy < view_rows; sy++)
        for (int sx = 0; sx < cols; sx++)
            render_cell(g, view, cols, sx, sy, cam_x + sx, cam_y + sy);

    if (status_rows)
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
