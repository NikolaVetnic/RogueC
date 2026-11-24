#include "test.h"

#include "vec2.h"

int test_failures = 0;
int test_checks = 0;

static void test_vec2_equal(void)
{
    Vec2 a = {1, 2}, b = {1, 2}, c = {2, 1};
    CHECK(vec2equal(a, b));
    CHECK(!vec2equal(a, c));
}

int main(void)
{
    RUN(test_vec2_equal);

    printf("\n%d checks, %d failed\n", test_checks, test_failures);
    return test_failures == 0 ? 0 : 1;
}
