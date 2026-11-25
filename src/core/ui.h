/* Screens and keys, the same in every frontend: the keymap, the map screen,
 * help and message history. Frontends pass key presses to ui_key and draw the
 * cells ui_render fills. */
#ifndef ROGUEC_UI_H
#define ROGUEC_UI_H

#include "core/game.h"
#include "core/render.h"

typedef enum
{
    SCREEN_MAP,
    SCREEN_HELP,
    SCREEN_LOG,
} Screen;

/* A key is a Unicode character (Esc = 27, Enter = '\n') or one of these codes
 * for keys that type none. */
enum
{
    UI_KEY_UP = 0x110000, /* past the last code point */
    UI_KEY_DOWN,
    UI_KEY_LEFT,
    UI_KEY_RIGHT,
    UI_KEY_HOME,
    UI_KEY_END,
    UI_KEY_PAGE_UP,
    UI_KEY_PAGE_DOWN,
    UI_KEY_CENTER, /* numpad 5 without Num Lock */
    UI_KEY_OTHER,  /* any other key: closes a screen */
};

typedef struct
{
    Screen screen;
    int scroll; /* message history: lines hidden below the newest shown */
} Ui;

void ui_init(Ui *ui);
/* Handles one key press. The game may have stopped afterwards (g->running). */
void ui_key(Ui *ui, Game *g, int key);
/* Fills cells[rows * cols] with the current screen. Keeps ui->scroll in range
 * for this size, so it takes Ui by pointer. */
void ui_render(Ui *ui, const Game *g, Cell *cells, int cols, int rows);

#endif
