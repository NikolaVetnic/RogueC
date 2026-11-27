#include "core/path.h"

#include <stdlib.h>

static bool passable(const Map *m, int x, int y, bool known_only)
{
    return map_walkable(m, x, y) && (!known_only || map_is_explored(m, x, y));
}

static bool distances(const Map *m, Vec2 from, int *dist, bool known_only)
{
    int n = m->w * m->h;
    int *queue = malloc((size_t)n * sizeof *queue);
    if (!queue)
        return false;

    for (int i = 0; i < n; i++)
        dist[i] = -1;

    int head = 0, tail = 0;
    if (passable(m, from.x, from.y, known_only))
    {
        dist[from.y * m->w + from.x] = 0;
        queue[tail++] = from.y * m->w + from.x;
    }

    while (head < tail)
    {
        int i = queue[head++];
        int x = i % m->w, y = i / m->w;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
            {
                int nx = x + dx, ny = y + dy;
                if ((dx == 0 && dy == 0) || !passable(m, nx, ny, known_only))
                    continue;
                int j = ny * m->w + nx;
                if (dist[j] < 0)
                {
                    dist[j] = dist[i] + 1;
                    queue[tail++] = j;
                }
            }
    }

    free(queue);
    return true;
}

bool path_distances(const Map *m, Vec2 from, int *dist)
{
    return distances(m, from, dist, false);
}

bool path_distances_known(const Map *m, Vec2 from, int *dist)
{
    return distances(m, from, dist, true);
}
