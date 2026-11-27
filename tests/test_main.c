#include "test.h"

int test_failures = 0;
int test_checks = 0;

int main(void)
{
    run_rng_tests();
    run_line_tests();
    run_map_tests();
    run_gen_cave_tests();
    run_fov_tests();
    run_game_tests();
    run_render_tests();
    run_msglog_tests();
    run_ui_tests();
    run_dungeon_tests();
    run_monster_tests();
    run_item_tests();
    run_progression_tests();
    run_save_tests();
    run_touch_tests();

    printf("\n%d checks, %d failed\n", test_checks, test_failures);
    return test_failures == 0 ? 0 : 1;
}
