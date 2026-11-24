#include "core/rng.h"

#include <assert.h>

/* Odd stream increment; any odd constant selects one of 2^63 independent streams. */
#define RNG_STREAM 0xda3e39cb94b95bdbULL

void rng_seed(Rng *rng, uint64_t seed)
{
    rng->state = 0;
    rng->inc = (RNG_STREAM << 1u) | 1u;
    rng_next(rng);
    rng->state += seed;
    rng_next(rng);
}

uint32_t rng_next(Rng *rng)
{
    uint64_t old = rng->state;
    rng->state = old * 6364136223846793005ULL + rng->inc;
    uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rot = (uint32_t)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31u));
}

uint32_t rng_below(Rng *rng, uint32_t n)
{
    assert(n > 0);
    /* Reject the low values that would make `r % n` favour small results. */
    uint32_t threshold = -n % n;
    for (;;)
    {
        uint32_t r = rng_next(rng);
        if (r >= threshold)
            return r % n;
    }
}

int rng_range(Rng *rng, int lo, int hi)
{
    assert(lo <= hi);
    return lo + (int)rng_below(rng, (uint32_t)(hi - lo) + 1u);
}

bool rng_chance(Rng *rng, int percent)
{
    return (int)rng_below(rng, 100) < percent;
}
