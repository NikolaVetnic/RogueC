/* Saved games. No I/O: game_save gives text the frontend stores, game_load
 * takes it back. */
#ifndef ROGUEC_SAVE_H
#define ROGUEC_SAVE_H

#include <stdbool.h>

#include "core/game.h"

/* The game as text (letters, digits, '+', '/' and '='), malloc'd: free it.
 * NULL if out of memory or the game is over (nothing to resume). */
char *game_save(const Game *g);
/* Restores a game saved by game_save into *g, which must not hold a game.
 * Returns false and leaves *g alone if the text is damaged, was saved by
 * another version, or memory runs out. */
bool game_load(Game *g, const char *text);

#endif
