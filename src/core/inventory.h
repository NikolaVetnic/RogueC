/* The player's pack and what items do. */
#ifndef ROGUEC_INVENTORY_H
#define ROGUEC_INVENTORY_H

#include "core/game.h"

/* Adds to the pack, stacking with a like item. Returns its pack index, or -1
 * if the pack is full. */
int inv_add(Player *p, Item it);
/* After the player moves: takes gold, says what lies here. */
void inv_step_on(Game *g);
/* Each returns true if it used a turn. */
bool inv_pick_up(Game *g);
bool inv_drop(Game *g, int i);
/* Eat, drink, read, wield, wear, or take off/put away what's equipped. */
bool inv_use(Game *g, int i);

#endif
