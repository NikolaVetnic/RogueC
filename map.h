#ifndef MAP
#define MAP

#include <ncurses.h>
#include <stdlib.h>
#include <time.h>

#include "tiles.h"
#include "vec2.h"

int **init_map(void);
int do_sim_step(int **map);
int count_alive_neighbors(int **m, int x, int y);
int **cp_map(int **map);
int draw_map(int **map);

#endif
