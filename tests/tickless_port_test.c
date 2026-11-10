/* Link the production Cortex-M4F port, with only scheduler/time-source inputs
 * substituted. The Python runner supplies SysTick MMIO values at wakeup. */
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

uint32_t SystemCoreClock = 100000000U;
volatile uint32_t scenario, stepped_ticks, sleep_calls, wake_calls;
extern void vPortSetupTimerInterrupt(void);
extern void vPortSuppressTicksAndSleep(TickType_t ticks);

eSleepModeStatus eTaskConfirmSleepModeStatus(void)
{
    return scenario == 0 ? eAbortSleep : eStandardSleep;
}
void vTaskStepTick(TickType_t ticks) { stepped_ticks = ticks; }
void PreSleepProcessing(uint32_t ticks) { (void)ticks; ++sleep_calls; }
void PostSleepProcessing(uint32_t ticks) { (void)ticks; ++wake_calls; }

int test_main(void)
{
    /* The runner supplies scenario and requested idle time before entry. */
    vPortSetupTimerInterrupt();
    *(volatile uint32_t *)0xE000E018 = 50000U;
    vPortSuppressTicksAndSleep(scenario == 3 ? 500U : 10U);
    return 0;
}
