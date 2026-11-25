#include "test.h"

#include "core/rng.h"

static void same_seed_same_sequence(void)
{
    Rng a, b;
    rng_seed(&a, 42);
    rng_seed(&b, 42);
    for (int i = 0; i < 1000; i++)
        CHECK(rng_next(&a) == rng_next(&b));
}

static void different_seeds_differ(void)
{
    Rng a, b;
    rng_seed(&a, 1);
    rng_seed(&b, 2);
    int same = 0;
    for (int i = 0; i < 100; i++)
        same += rng_next(&a) == rng_next(&b);
    CHECK(same < 5);
}

static void range_stays_in_bounds_and_hits_both_ends(void)
{
    Rng r;
    rng_seed(&r, 7);
    bool lo_hit = false, hi_hit = false, in_bounds = true;
    for (int i = 0; i < 10000; i++)
    {
        int v = rng_range(&r, -3, 3);
        in_bounds &= v >= -3 && v <= 3;
        lo_hit |= v == -3;
        hi_hit |= v == 3;
    }
    CHECK(in_bounds);
    CHECK(lo_hit && hi_hit);
}

static void chance_matches_percent(void)
{
    Rng r;
    rng_seed(&r, 9);
    int hits = 0;
    for (int i = 0; i < 100000; i++)
        hits += rng_chance(&r, 25);
    CHECK(hits > 24000 && hits < 26000);

    CHECK(!rng_chance(&r, 0));
    CHECK(rng_chance(&r, 100));
}

static void streams_differ_and_repeat(void)
{
    Rng a, b, c;
    rng_seed_stream(&a, 42, 1);
    rng_seed_stream(&b, 42, 1);
    rng_seed_stream(&c, 42, 2);
    int same_ab = 0, same_ac = 0;
    for (int i = 0; i < 100; i++)
    {
        uint32_t x = rng_next(&a), y = rng_next(&b), z = rng_next(&c);
        same_ab += x == y;
        same_ac += x == z;
    }
    CHECK(same_ab == 100);
    CHECK(same_ac < 5);
}

void run_rng_tests(void)
{
    RUN(streams_differ_and_repeat);
    RUN(same_seed_same_sequence);
    RUN(different_seeds_differ);
    RUN(range_stays_in_bounds_and_hits_both_ends);
    RUN(chance_matches_percent);
}
