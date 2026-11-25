#include "core/ai.h"

static const Vec2 DIRS[8] = {
    {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1},
};

static bool adjacent(Vec2 a, Vec2 b)
{
    int dx = a.x - b.x, dy = a.y - b.y;
    return (dx || dy) && dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1;
}

/* Walkable and nobody there. */
static bool free_cell(Game *g, Vec2 p)
{
    return map_walkable(g->map, p.x, p.y) && !vec2_eq(p, g->player.pos) &&
           level_monster_at(game_level(g), p) < 0;
}

/* Steps to the free neighbour closest to the player (ties at random); if
 * other monsters block every closer cell, sideways to one just as close. */
static void step_towards(Game *g, Monster *m, const int *dist)
{
    int w = g->map->w;
    int here = dist[m->pos.y * w + m->pos.x], best = here;
    Vec2 closer[8], sideways[8];
    int n_closer = 0, n_sideways = 0;

    for (int k = 0; k < 8; k++)
    {
        Vec2 n = vec2_add(m->pos, DIRS[k]);
        int d = map_in_bounds(g->map, n.x, n.y) ? dist[n.y * w + n.x] : -1;
        if (d < 0 || !free_cell(g, n))
            continue;
        if (d < best)
        {
            best = d;
            n_closer = 0;
        }
        if (d == best && d < here)
            closer[n_closer++] = n;
        else if (d == here)
            sideways[n_sideways++] = n;
    }

    if (n_closer > 0)
        m->pos = closer[rng_range(&g->rng, 0, n_closer - 1)];
    else if (n_sideways > 0)
        m->pos = sideways[rng_range(&g->rng, 0, n_sideways - 1)];
}

/* Keeps its heading most of the time, else turns to a random free direction. */
static void wander(Game *g, Monster *m)
{
    Vec2 ahead = vec2_add(m->pos, m->heading);
    bool has_heading = m->heading.x || m->heading.y;
    if (has_heading && free_cell(g, ahead) && rng_chance(&g->rng, 80))
    {
        m->pos = ahead;
        return;
    }
    int start = rng_range(&g->rng, 0, 7);
    for (int k = 0; k < 8; k++)
    {
        Vec2 dir = DIRS[(start + k) % 8];
        if (free_cell(g, vec2_add(m->pos, dir)))
        {
            m->heading = dir;
            m->pos = vec2_add(m->pos, dir);
            return;
        }
    }
}

void ai_act(Game *g, Monster *m, const int *dist)
{
    /* The player's view stands in for the monster's: sight is symmetric enough. */
    bool sees = map_is_visible(g->map, m->pos.x, m->pos.y);

    if (!m->awake)
    {
        if (!sees || !rng_chance(&g->rng, WAKE_PCT))
            return;
        m->awake = true;
    }
    if (sees)
        m->track = TRACK_TICKS;
    else if (m->track > 0)
        m->track--;

    if (adjacent(m->pos, g->player.pos))
        game_monster_attack(g, m);
    else if ((monster_info(m)->flags & MON_ERRATIC) && rng_chance(&g->rng, 50))
        wander(g, m);
    else if (m->track > 0 && dist && dist[m->pos.y * g->map->w + m->pos.x] > 0)
        step_towards(g, m, dist);
    else
        wander(g, m);
}
