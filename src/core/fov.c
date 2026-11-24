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
}
