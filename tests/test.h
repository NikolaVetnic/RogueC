/* Minimal unit-test helpers: CHECK records a failure and continues. */
#ifndef ROGUEC_TEST_H
#define ROGUEC_TEST_H

#include <stdio.h>

extern int test_failures;
extern int test_checks;

#define CHECK(cond)                                                                                \
    do                                                                                             \
    {                                                                                              \
        test_checks++;                                                                             \
        if (!(cond))                                                                               \
        {                                                                                          \
            test_failures++;                                                                       \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                      \
        }                                                                                          \
    } while (0)

#define RUN(test_fn)                                                                               \
    do                                                                                             \
    {                                                                                              \
        int before_ = test_failures;                                                               \
        test_fn();                                                                                 \
        printf("%s %s\n", test_failures == before_ ? "ok  " : "FAIL", #test_fn);                   \
    } while (0)

/* One per test file. */
void run_rng_tests(void);
void run_line_tests(void);
void run_map_tests(void);
void run_gen_cave_tests(void);
void run_fov_tests(void);
void run_game_tests(void);
void run_render_tests(void);
void run_msglog_tests(void);
void run_ui_tests(void);
void run_dungeon_tests(void);

#endif
