#include "WristWake.h"
volatile uint32_t tests_passed;
#define CHECK(x) do { if (!(x)) return __LINE__; ++tests_passed; } while (0)
int test_main(void)
{
    WristWake s = {0};
    for (int i = 0; i < 3; ++i) CHECK(!WristWake_Update(&s, true, 0, 0));
    CHECK(s.known && s.up);
    for (int i = 0; i < 8; ++i) CHECK(!WristWake_Update(&s, true, 0.7f, 0));
    CHECK(s.up);
    for (int i = 0; i < 3; ++i) CHECK(!WristWake_Update(&s, true, 1.2f, 0));
    CHECK(!s.up);
    CHECK(!WristWake_Update(&s, true, 0, 0));
    CHECK(!WristWake_Update(&s, false, 0, 0));
    CHECK(!WristWake_Update(&s, true, 0, 0));
    CHECK(!WristWake_Update(&s, true, 0, 0));
    CHECK(WristWake_Update(&s, true, 0, 0));
    for (int i = 0; i < 10; ++i) CHECK(!WristWake_Update(&s, true, 0, 0));
    s = (WristWake){0};
    for (int i = 0; i < 10; ++i) {
        CHECK(!WristWake_Update(&s, true, i % 2 ? 0 : 1.2f, 0));
        CHECK(!s.known);
    }
    s = (WristWake){0};
    for (int i = 0; i < 3; ++i) CHECK(!WristWake_Update(&s, true, 3.14f, 0));
    CHECK(!s.up);
    return 0;
}
