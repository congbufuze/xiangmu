#ifndef APP_POWER_H
#define APP_POWER_H
#include <stdint.h>
#define APP_UI_NOTIFY (1U << 0)
#define APP_UI_TOUCH_IRQ (1U << 1)
#define APP_KEY_EDGE (1U << 0)
void AppPower_StartRuntimeClock(void);
void AppPower_NotifyUI(void);
/* CubeMX's built-in tickless hooks; the pre-hook executes WFI. */
void PreSleepProcessing(uint32_t expected_ticks);
void PostSleepProcessing(uint32_t expected_ticks);
#endif
