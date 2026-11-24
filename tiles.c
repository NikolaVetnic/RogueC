#include "tiles.h"

char *T_WALL = "█";
char *T_GROUND = ".";
char *T_VISIBLE = " ";
char *T_PC = "@";

char *t_wall(void)
{
    return T_WALL;
}
char *t_ground(void)
{
    return T_GROUND;
}
char *t_visible(void)
{
    return T_VISIBLE;
}
char *t_pc(void)
{
    return T_PC;
}
