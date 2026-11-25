/* Screens and keys, the same in every frontend: the keymap, the map screen,
 * help and message history. Frontends pass key presses to ui_key and draw the
 * cells ui_render fills. */
#ifndef ROGUEC_UI_H
#define ROGUEC_UI_H

#include "core/game.h"
#include "core/render.h"
#include "core/score.h"

typedef enum
{
    SCREEN_MAP,
    SCREEN_HELP,
    SCREEN_LOG,
    SCREEN_END,  /* died or won: score and high scores */
    SCREEN_PACK, /* the inventory: choose an item to use or drop */
} Screen;

typedef enum
{
    PACK_USE,
    PACK_DROP,
} PackPurpose;

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

enum
{
    UI_TICK_MS = 300, /* real time: one tick (a normal-speed move) this often */
};

typedef struct
{
    Screen screen;
    int scroll; /* message history: lines hidden below the newest shown */
    PackPurpose pack_purpose;
    Action pending;      /* real time: pressed before the player was ready */
    int pending_item;    /* ... and its pack item, for use and drop */
    int clock_ms;        /* real time: time since the last tick */
    bool new_game;       /* the player asked for a new game: the frontend starts one */
    ScoreTable *scores;  /* the frontend's high scores, or NULL */
    bool scores_changed; /* a score was added: the frontend saves the table */
    int score_rank;      /* this game's place in the table, or -1 */
} Ui;

/* `scores` (may be NULL) gets this game's score when it ends. */
void ui_init(Ui *ui, ScoreTable *scores);
/* Handles one key press. The game may have stopped afterwards (g->running),
 * or ui->new_game may ask for a new one. */
void ui_key(Ui *ui, Game *g, int key);
/* Call every frame with the time since the last call. In real time it runs
 * the world's ticks and the buffered move; on other screens time stands still. */
void ui_update(Ui *ui, Game *g, int elapsed_ms);
/* Fills cells[rows * cols] with the current screen. Keeps ui->scroll in range
 * for this size, so it takes Ui by pointer. */
void ui_render(Ui *ui, const Game *g, Cell *cells, int cols, int rows);

#endif
