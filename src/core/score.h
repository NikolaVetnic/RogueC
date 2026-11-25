/* Scores and the high-score table. No I/O: frontends store the text from
 * scores_format and hand it back to scores_parse. */
#ifndef ROGUEC_SCORE_H
#define ROGUEC_SCORE_H

#include <stdbool.h>

#include "core/game.h"

enum
{
    SCORES_MAX = 10,
    CAUSE_SIZE = 48,
    WIN_BONUS = 2000,
};

typedef struct
{
    int points;
    int deepest;
    int level;
    int turns;
    bool won;
    char cause[CAUSE_SIZE]; /* "killed by an orc on depth 5" */
} Score;

typedef struct
{
    Score entries[SCORES_MAX]; /* best first */
    int count;
} ScoreTable;

/* gold + 10 x experience + 100 x deepest depth, + WIN_BONUS for a win. */
Score score_of(const Game *g);
/* Inserts in order. Returns its rank (0 = best), or -1 if it didn't make the table. */
int scores_add(ScoreTable *t, Score s);
/* One score per line: "points deepest level turns won cause". Bad lines are skipped. */
void scores_parse(ScoreTable *t, const char *text);
/* Writes the table in that form; returns false if `size` is too small. */
bool scores_format(const ScoreTable *t, char *out, int size);

#endif
