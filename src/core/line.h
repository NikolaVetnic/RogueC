/* Bresenham's line between two cells, in all eight octants. */
#ifndef ROGUEC_LINE_H
#define ROGUEC_LINE_H

#include <stdbool.h>

#include "core/vec2.h"

/* Called for each cell of the line in order, both ends included. Return false to stop. */
typedef bool (*LineVisitor)(Vec2 cell, void *ctx);

/* Walks from a to b; returns true if the walk reached b without being stopped. */
bool line_walk(Vec2 a, Vec2 b, LineVisitor visit, void *ctx);

#endif
