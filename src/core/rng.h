/* Seeded pseudo-random numbers (PCG32, pcg-random.org).
 * The same seed gives the same sequence on every platform, so levels are reproducible. */
#ifndef ROGUEC_RNG_H
#define ROGUEC_RNG_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint64_t state;
    uint64_t inc;
} Rng;

void rng_seed(Rng *rng, uint64_t seed);
uint32_t rng_next(Rng *rng);
/* Uniform in [0, n); n must be > 0. */
uint32_t rng_below(Rng *rng, uint32_t n);
/* Uniform in [lo, hi]; requires lo <= hi. */
int rng_range(Rng *rng, int lo, int hi);
/* True with probability percent / 100. */
bool rng_chance(Rng *rng, int percent);

#endif
