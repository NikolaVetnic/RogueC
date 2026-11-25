#include "core/fov.h"

#include "core/line.h"

typedef struct
{
    Map *map;
    Vec2 origin;
    int radius_sq;
} Ray;

static bool light_cell(Vec2 c, void *ctx)
{
    Ray *ray = ctx;
    int dx = c.x - ray->origin.x, dy = c.y - ray->origin.y;

    if (!map_in_bounds(ray->map, c.x, c.y) || dx * dx + dy * dy > ray->radius_sq)
        return false;

    map_mark_seen(ray->map, c.x, c.y);
    return vec2_eq(c, ray->origin) || map_transparent(ray->map, c.x, c.y);
}

static int sign(int v)
{
    return (v > 0) - (v < 0);
}

static bool visible_floor(const Map *m, int x, int y)
{
    return map_is_visible(m, x, y) && map_transparent(m, x, y);
}

/* A ray to a wall can be stopped by another wall first, although the floor in
 * front of it is in view. Light such walls: an opaque cell in range whose
 * neighbour on the origin's side (straight or diagonal) is visible floor.
 * Only floor counts, so a wall behind a wall stays dark. */
static void light_facing_walls(Map *m, Vec2 o, int radius_sq, int radius)
{
    for (int y = o.y - radius; y <= o.y + radius; y++)
        for (int x = o.x - radius; x <= o.x + radius; x++)
        {
            int dx = x - o.x, dy = y - o.y;
            if (!map_in_bounds(m, x, y) || dx * dx + dy * dy > radius_sq ||
                map_transparent(m, x, y) || map_is_visible(m, x, y))
                continue;

            int sx = -sign(dx), sy = -sign(dy); /* one step towards the origin */
            if ((sx && visible_floor(m, x + sx, y)) || (sy && visible_floor(m, x, y + sy)) ||
                (sx && sy && visible_floor(m, x + sx, y + sy)))
                map_mark_seen(m, x, y);
        }
}

void fov_compute(Map *m, Vec2 origin, int radius)
{
    /* r^2 + r instead of r^2 rounds the circle's edge: no single-cell bumps at the axes. */
    Ray ray = {.map = m, .origin = origin, .radius_sq = radius * radius + radius};

    map_clear_visible(m);
    map_mark_seen(m, origin.x, origin.y);

    for (int i = -radius; i <= radius; i++)
    {
        line_walk(origin, vec2(origin.x + i, origin.y - radius), light_cell, &ray);
        line_walk(origin, vec2(origin.x + i, origin.y + radius), light_cell, &ray);
        line_walk(origin, vec2(origin.x - radius, origin.y + i), light_cell, &ray);
        line_walk(origin, vec2(origin.x + radius, origin.y + i), light_cell, &ray);
    }

    light_facing_walls(m, origin, ray.radius_sq, radius);
}
