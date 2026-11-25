/* What monsters do on their turn. */
#ifndef ROGUEC_AI_H
#define ROGUEC_AI_H

#include "core/game.h"

enum
{
    WAKE_PCT = 35,    /* chance per action that an asleep monster in view wakes */
    TRACK_TICKS = 20, /* actions a monster keeps hunting after losing sight */
};

/* One action of monster m on the current level: wake up, attack when next to
 * the player, step towards the player while hunting (downhill on `dist`, the
 * steps to the player from each cell; NULL if unknown), else wander. */
void ai_act(Game *g, Monster *m, const int *dist);

#endif
