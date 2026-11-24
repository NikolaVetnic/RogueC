/* What each screen cell shows. Frontends only map Styles to colours and draw. */
#ifndef ROGUEC_RENDER_H
#define ROGUEC_RENDER_H

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
    STYLE_COUNT
} Style;

typedef struct
{
    uint32_t glyph; /* Unicode code point */
    uint8_t style;  /* Style */
} Cell;

/* Fills cells[rows * cols] (row-major): the map centred on the player, with a
 * status line on the last row when there are at least two rows. */
void render_game(const Game *g, Cell *cells, int cols, int rows);

/* Writes code point cp as NUL-terminated UTF-8; returns the number of bytes. */
int utf8_encode(uint32_t cp, char out[5]);

#endif
