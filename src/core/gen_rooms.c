#include "core/gen_rooms.h"

#include <stdlib.h>

#include "core/vec2.h"

const RoomParams ROOM_DEFAULTS = {
    .max_rooms = 12,
    .attempts = 400,
    .min_w = 4,
    .max_w = 14,
    .min_h = 3,
    .max_h = 7,
    .extra_link_pct = 20,
};

typedef struct
{
    int x, y, w, h; /* interior */
} Rect;

/* Rooms keep at least two walls between them, so each has its own wall. */
static bool overlaps(Rect a, Rect b)
{
    return a.x - 2 < b.x + b.w && b.x - 2 < a.x + a.w && a.y - 2 < b.y + b.h && b.y - 2 < a.y + a.h;
}

static void carve_rect(Map *m, Rect r)
{
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++)
            map_set(m, x, y, TILE_FLOOR);
}

static Vec2 centre(Rect r)
{
    return vec2(r.x + r.w / 2, r.y + r.h / 2);
}

static void carve_h(Map *m, int x0, int x1, int y)
{
    for (int x = x0 < x1 ? x0 : x1; x <= (x0 < x1 ? x1 : x0); x++)
        map_set(m, x, y, TILE_FLOOR);
}

static void carve_v(Map *m, int y0, int y1, int x)
{
    for (int y = y0 < y1 ? y0 : y1; y <= (y0 < y1 ? y1 : y0); y++)
        map_set(m, x, y, TILE_FLOOR);
}

/* L-shaped corridor between the rooms' centres, the bend on either side. */
static void link(Map *m, Rng *rng, Rect a, Rect b)
{
    Vec2 p = centre(a), q = centre(b);
    if (rng_chance(rng, 50))
    {
        carve_h(m, p.x, q.x, p.y);
        carve_v(m, p.y, q.y, q.x);
    }
    else
    {
        carve_v(m, p.y, q.y, p.x);
        carve_h(m, p.x, q.x, q.y);
    }
}

static int by_x(const void *a, const void *b)
{
    const Rect *ra = a, *rb = b;
    return (ra->x > rb->x) - (ra->x < rb->x);
}

int gen_rooms(Map *m, Rng *rng, const RoomParams *p)
{
    Rect rooms[ROOMS_MAX];
    int count = 0;
    int max_rooms = p->max_rooms < ROOMS_MAX ? p->max_rooms : ROOMS_MAX;

    map_fill(m, TILE_WALL);
    for (int i = 0; i < p->attempts && count < max_rooms; i++)
    {
        Rect r;
        r.w = rng_range(rng, p->min_w, p->max_w);
        r.h = rng_range(rng, p->min_h, p->max_h);
        if (r.w > m->w - 2 || r.h > m->h - 2)
            continue;
        r.x = rng_range(rng, 1, m->w - 1 - r.w);
        r.y = rng_range(rng, 1, m->h - 1 - r.h);

        bool free_spot = true;
        for (int j = 0; j < count && free_spot; j++)
            free_spot = !overlaps(r, rooms[j]);
        if (free_spot)
            rooms[count++] = r;
    }
    if (count < 2)
        return 0;

    /* Left to right, each room joined to the next: all connected, corridors short. */
    qsort(rooms, (size_t)count, sizeof rooms[0], by_x);
    for (int i = 0; i < count; i++)
        carve_rect(m, rooms[i]);
    for (int i = 1; i < count; i++)
        link(m, rng, rooms[i - 1], rooms[i]);
    for (int i = 0; i < count; i++)
        if (rng_chance(rng, p->extra_link_pct))
            link(m, rng, rooms[i], rooms[rng_range(rng, 0, count - 1)]);

    return count;
}
