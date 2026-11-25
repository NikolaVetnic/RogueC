/* What each screen cell shows, and the colours of each Style. Frontends only
 * draw cells in these colours. */
#ifndef ROGUEC_RENDER_H
#define ROGUEC_RENDER_H

#include <stdbool.h>
#include <stdint.h>

#include "core/game.h"

typedef enum
{
    STYLE_DEFAULT,
    STYLE_FLOOR_LIT,  /* floor in view */
    STYLE_FLOOR_SEEN, /* floor remembered, out of view */
    STYLE_WALL_LIT,
    STYLE_WALL_SEEN,
    STYLE_PLAYER,
    STYLE_STATUS,
    STYLE_TEXT,     /* messages since the last action, screen text */
    STYLE_TEXT_DIM, /* older messages, hints */
    STYLE_HEADING,
    STYLE_KEY, /* key names on the help screen */
    STYLE_COUNT
} Style;

typedef struct
{
    uint32_t glyph; /* Unicode code point */
    uint8_t style;  /* Style */
} Cell;

typedef struct
{
    uint8_t r, g, b;
} Rgb;

typedef struct
{
    Rgb fg;
    Rgb bg;
    bool has_bg; /* false: the screen's background */
} StyleColor;

/* The palette: one source for every frontend. */
extern const Rgb PALETTE_BACKGROUND;
extern const StyleColor STYLE_COLORS[STYLE_COUNT];

#define GLYPH_WALL 0x2588u /* full block; graphical frontends draw it as a filled cell */
#define GLYPH_FLOOR '.'
#define GLYPH_PLAYER '@'

/* Map screen: fills cells[rows * cols] (row-major) with the map centred on the
 * player, the two newest messages on top when there are at least 10 rows, and
 * a status line on the last row when there are at least two. */
void render_game(const Game *g, Cell *cells, int cols, int rows);

/* Sets every cell to a blank of `style`. */
void render_clear(Cell *cells, int cols, int rows, Style style);
/* Writes ASCII text from (x, y), cut at the right edge; returns the column after it. */
int render_text(Cell *cells, int cols, int rows, int x, int y, const char *text, Style style);
/* Message text with its repeat count, e.g. "Rock blocks the way. (x3)". */
void render_message_text(const Message *m, char *out, int size);

/* Writes code point cp as NUL-terminated UTF-8; returns the number of bytes. */
int utf8_encode(uint32_t cp, char out[5]);

#endif
