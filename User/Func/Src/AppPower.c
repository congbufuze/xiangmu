#include "AppPower.h"
#include "AppServices.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "key.h"
#include "CST816.h"

extern TIM_HandleTypeDef htim11;
extern osThreadId_t KeyTaskHandle, UITaskHandle;
static volatile uint32_t runtime_clock;
static uint32_t tick_offset;
_Static_assert(configTICK_RATE_HZ == 1000, "Runtime HAL clock requires 1ms RTOS ticks");

/* TIM11 remains the CubeMX HAL timebase during startup. Once the RTOS and
 * hardware are ready, its compensated tick replaces the redundant 1kHz IRQ.
 * This adapter is for Sleep only: STOP needs an independent elapsed-time clock.
 */
void AppPower_StartRuntimeClock(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if (!runtime_clock) {
        uint32_t hal_ms = uwTick;
        if (__HAL_TIM_GET_FLAG(&htim11, TIM_FLAG_UPDATE)) ++hal_ms;
        tick_offset = hal_ms - (uint32_t)xTaskGetTickCount();
        HAL_SuspendTick();
        __HAL_TIM_DISABLE(&htim11);
        __HAL_TIM_CLEAR_FLAG(&htim11, TIM_FLAG_UPDATE);
        HAL_NVIC_ClearPendingIRQ(TIM1_TRG_COM_TIM11_IRQn);
        __DMB();
        runtime_clock = 1U;
        g_app_diagnostics.runtime_tick_active = true;
    }
    __set_PRIMASK(mask);
}

uint32_t HAL_GetTick(void)
{
    if (!runtime_clock) return uwTick;
    uint32_t ticks = __get_IPSR() ? (uint32_t)xTaskGetTickCountFromISR() :
                                  (uint32_t)xTaskGetTickCount();
    return tick_offset + ticks;
}

extern void xPortSysTickHandler(void);
void SysTick_Handler(void)
{
    /* Do not read CTRL: its COUNTFLAG belongs to vPortSuppressTicksAndSleep.
     * The default CMSIS wrapper clears it before this older port can inspect it. */
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        xPortSysTickHandler();
}

void PreSleepProcessing(uint32_t expected_ticks)
{
    /* The standard port keeps SysTick clocked and compensates elapsed ticks.
     * Never set SLEEPDEEP here: that would stop its time reference. */
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    ++g_app_diagnostics.tickless_entries;
    (void)expected_ticks; /* CubeMX sets this argument to zero. */
    g_app_diagnostics.tickless_last_programmed_ticks =
        (SysTick->LOAD + 1U) / (SystemCoreClock / configTICK_RATE_HZ);
    __DSB();
    __WFI();
    __ISB();
}

void PostSleepProcessing(uint32_t expected_ticks)
{
    (void)expected_ticks;
    ++g_app_diagnostics.tickless_exits;
}

void AppPower_NotifyUI(void)
{
    if (UITaskHandle) (void)osThreadFlagsSet(UITaskHandle, APP_UI_NOTIFY);
}

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    /* No I2C, LVGL, delay or event-group timer-queue operations in an ISR. */
    if (pin == KEY1_PIN && KeyTaskHandle) {
        ++g_app_diagnostics.key_irq_count;
        (void)osThreadFlagsSet(KeyTaskHandle, APP_KEY_EDGE);
    } else if (pin == TOUCH_INT_PIN && UITaskHandle) {
        ++g_app_diagnostics.touch_irq_count;
        (void)osThreadFlagsSet(UITaskHandle, APP_UI_TOUCH_IRQ);
    }
}
