#include "LowPowerPolicy.h"
volatile uint32_t tests_passed;
#define CHECK(x) do { if (!(x)) return __LINE__; ++tests_passed; } while (0)
int test_main(void)
{
    CHECK(AppEnv_IntervalMs(false) == 500U);
    CHECK(AppEnv_IntervalMs(true) == 5000U);
    CHECK(AppSave_WaitMs(100, 0, 0, 0, false, 0) == UINT32_MAX);
    CHECK(AppSave_WaitMs(1000, 1, 0, 1000, false, 0) == 1000);
    CHECK(AppSave_WaitMs(1999, 1, 0, 1000, false, 0) == 1);
    CHECK(AppSave_WaitMs(2000, 1, 0, 1000, false, 0) == 0);
    CHECK(AppSave_WaitMs(3000, 2, 1, 2900, false, 0) == 900);
    CHECK(AppSave_WaitMs(3000, 2, 1, 1000, true, 2500) == 4500);
    CHECK(AppSave_WaitMs(7499, 2, 1, 1000, true, 2500) == 1);
    CHECK(AppSave_WaitMs(7500, 2, 1, 1000, true, 2500) == 0);
    CHECK(AppSave_WaitMs(7500, 2, 2, 1000, true, 2500) == UINT32_MAX);
    CHECK(AppSave_WaitMs(50, 1, 0, UINT32_MAX - 449, false, 0) == 500);
    CHECK(AppSave_WaitMs(550, 1, 0, UINT32_MAX - 449, false, 0) == 0);
    CHECK(AppSave_WaitMs(50, 1, 0, UINT32_MAX - 2000, true,
                        UINT32_MAX - 449) == 4500);
    CHECK(AppSave_WaitMs(999, 1, 0, 900, true, 0) == 4001);
    CHECK(AppSave_WaitMs(5000, 1, 0, 4900, true, 0) == 900);
    return 0;
}
