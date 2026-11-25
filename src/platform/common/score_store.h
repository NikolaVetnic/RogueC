/* Keeps the high-score table between games: a file in the home directory
 * (terminal and desktop) or the browser's localStorage (web). */
#ifndef ROGUEC_SCORE_STORE_H
#define ROGUEC_SCORE_STORE_H

#include "core/score.h"

/* An empty table if there is nothing stored or it can't be read. */
void score_store_load(ScoreTable *t);
/* Silently does nothing if it can't be written. */
void score_store_save(const ScoreTable *t);

#endif
