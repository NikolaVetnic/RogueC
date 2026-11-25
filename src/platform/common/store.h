/* What the frontends keep between runs: high scores, options and the saved
 * game. Native builds use files in the home directory ($HOME/.roguec_scores,
 * .roguec_options, .roguec_save); the browser uses localStorage. Failures
 * (no home directory, storage full or blocked) are silent: the game goes on. */
#ifndef ROGUEC_STORE_H
#define ROGUEC_STORE_H

#include <stdbool.h>
#include <stdint.h>

#include "core/game.h"
#include "core/score.h"
#include "core/settings.h"

void store_load_scores(ScoreTable *t);
void store_save_scores(const ScoreTable *t);
Settings store_load_settings(void);
void store_save_settings(const Settings *s);
/* Saves the game, unless it is over. */
void store_save_game(const Game *g);
void store_delete_game(void);

/* Resumes the saved game if `resume` and there is one; otherwise starts a
 * new game from `seed`, in real time if the settings say so (and says so if a
 * save was there but couldn't be loaded). Returns false if out of memory. */
bool store_start_game(Game *g, bool resume, uint64_t seed, const Settings *s);

#endif
