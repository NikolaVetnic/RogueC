#include "core/line.h"

#include <stdlib.h>

bool line_walk(Vec2 a, Vec2 b, LineVisitor visit, void *ctx)
{
    int dx = abs(b.x - a.x);
    int dy = -abs(b.y - a.y);
    int sx = a.x < b.x ? 1 : -1;
    int sy = a.y < b.y ? 1 : -1;
    int err = dx + dy; /* error term for both axes at once */

    for (;;)
    {
        if (!visit(a, ctx))
            return false;
        if (vec2_eq(a, b))
            return true;

        int e2 = 2 * err;
        if (e2 >= dy) /* step in x */
        {
            err += dy;
            a.x += sx;
        }
        if (e2 <= dx) /* step in y */
        {
            err += dx;
            a.y += sy;
        }
    }
}
