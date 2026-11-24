/* 2D integer vector: map and screen coordinates. */
#ifndef ROGUEC_VEC2_H
#define ROGUEC_VEC2_H

#include <stdbool.h>

typedef struct
{
    int x;
    int y;
} Vec2;

static inline Vec2 vec2(int x, int y)
{
    return (Vec2){x, y};
}

static inline Vec2 vec2_add(Vec2 a, Vec2 b)
{
    return (Vec2){a.x + b.x, a.y + b.y};
}

static inline bool vec2_eq(Vec2 a, Vec2 b)
{
    return a.x == b.x && a.y == b.y;
}

#endif
