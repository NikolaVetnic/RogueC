#include "core/score.h"

#include <stdio.h>
#include <string.h>

/* Letters, digits and a few marks only: the text goes into files and a
 * JavaScript string unescaped. */
static void clean(char *s)
{
    for (; *s; s++)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') ||
              strchr(" ,.!-", *s)))
            *s = ' ';
}

Score score_of(const Game *g)
{
    const Player *p = &g->player;
    Score s = {
        .points = p->gold + 10 * p->xp + 100 * g->deepest + (g->won ? WIN_BONUS : 0),
        .deepest = g->deepest,
        .level = p->level,
        .turns = g->turn,
        .won = g->won,
    };
    if (g->won)
        snprintf(s.cause, sizeof s.cause, "escaped with %s", GOAL_NAME);
    else if (!g->dead)
        snprintf(s.cause, sizeof s.cause, "quit on depth %d", g->depth);
    else if (g->death == DEATH_STARVATION)
        snprintf(s.cause, sizeof s.cause, "starved on depth %d", g->depth);
    else
        snprintf(s.cause, sizeof s.cause, "killed by %s %s on depth %d", monster_article(g->killer),
                 MONSTERS[g->killer].name, g->depth);
    clean(s.cause);
    return s;
}

int scores_add(ScoreTable *t, Score s)
{
    clean(s.cause);
    int rank = 0;
    while (rank < t->count && t->entries[rank].points >= s.points)
        rank++;
    if (rank >= SCORES_MAX)
        return -1;
    int moved = (t->count < SCORES_MAX ? t->count : SCORES_MAX - 1) - rank;
    memmove(&t->entries[rank + 1], &t->entries[rank], (size_t)moved * sizeof t->entries[0]);
    t->entries[rank] = s;
    if (t->count < SCORES_MAX)
        t->count++;
    return rank;
}

void scores_parse(ScoreTable *t, const char *text)
{
    *t = (ScoreTable){0};
    while (text && *text)
    {
        const char *end = strchr(text, '\n');
        size_t len = end ? (size_t)(end - text) : strlen(text);
        char line[128];
        if (len < sizeof line)
        {
            memcpy(line, text, len);
            line[len] = '\0';
            Score s = {0};
            int won = 0, used = 0;
            if (sscanf(line, "%d %d %d %d %d %n", &s.points, &s.deepest, &s.level, &s.turns, &won,
                       &used) == 5 &&
                used > 0)
            {
                s.won = won != 0;
                snprintf(s.cause, sizeof s.cause, "%s", line + used);
                scores_add(t, s);
            }
        }
        text = end ? end + 1 : text + len;
    }
}

bool scores_format(const ScoreTable *t, char *out, int size)
{
    int at = 0;
    out[0] = '\0';
    for (int i = 0; i < t->count; i++)
    {
        const Score *s = &t->entries[i];
        int n = snprintf(out + at, (size_t)(size - at), "%d %d %d %d %d %s\n", s->points,
                         s->deepest, s->level, s->turns, s->won ? 1 : 0, s->cause);
        if (n < 0 || n >= size - at)
            return false;
        at += n;
    }
    return true;
}
