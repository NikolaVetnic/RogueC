#include "test.h"

#include <stdlib.h>

#include "core/line.h"

typedef struct
{
    Vec2 cells[64];
    int count;
    int stop_after; /* 0 = never stop */
} Trace;

static bool record(Vec2 c, void *ctx)
{
    Trace *t = ctx;
    t->cells[t->count++] = c;
    return t->stop_after == 0 || t->count < t->stop_after;
}

/* Every line from the origin to the 8 octant targets is contiguous, starts at
 * a, ends at b, and has max(|dx|, |dy|) + 1 cells. */
static void lines_are_contiguous_in_all_octants(void)
{
    static const Vec2 targets[] = {{7, 3},  {3, 7},  {-3, 7}, {-7, 3}, {-7, -3}, {-3, -7},
                                   {3, -7}, {7, -3}, {5, 0},  {0, -5}, {4, 4},   {-4, 4}};
    Vec2 a = {0, 0};

    for (size_t t = 0; t < sizeof targets / sizeof targets[0]; t++)
    {
        Vec2 b = targets[t];
        Trace tr = {0};
        CHECK(line_walk(a, b, record, &tr));
        CHECK(vec2_eq(tr.cells[0], a));
        CHECK(vec2_eq(tr.cells[tr.count - 1], b));

        int len = abs(b.x) > abs(b.y) ? abs(b.x) : abs(b.y);
        CHECK(tr.count == len + 1);

        for (int i = 1; i < tr.count; i++)
        {
            CHECK(abs(tr.cells[i].x - tr.cells[i - 1].x) <= 1);
            CHECK(abs(tr.cells[i].y - tr.cells[i - 1].y) <= 1);
        }
    }
}

static void single_cell_line(void)
{
    Trace tr = {0};
    CHECK(line_walk(vec2(2, 2), vec2(2, 2), record, &tr));
    CHECK(tr.count == 1);
}

static void visitor_can_stop_the_walk(void)
{
    Trace tr = {.stop_after = 3};
    CHECK(!line_walk(vec2(0, 0), vec2(10, 0), record, &tr));
    CHECK(tr.count == 3);
}

void run_line_tests(void)
{
    RUN(lines_are_contiguous_in_all_octants);
    RUN(single_cell_line);
    RUN(visitor_can_stop_the_walk);
}
