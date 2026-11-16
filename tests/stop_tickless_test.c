/* Link BOTH production AppPower.c and the unmodified FreeRTOS port. Only
 * peripheral time/clock and scheduler boundaries are substituted. */
#include "AppPower.h"
#include "AppServices.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "rtc.h"

uint32_t SystemCoreClock = 100000000U;
volatile uint32_t uwTick;
TIM_HandleTypeDef htim11 = {.Instance = TIM11};
RTC_HandleTypeDef hrtc = {.Instance = RTC};
osThreadId_t KeyTaskHandle, UITaskHandle;
volatile AppDiagnostics g_app_diagnostics;
volatile uint32_t scenario, stepped_ticks, stop_calls;
volatile uint32_t final_reload, final_primask, rtc_disarmed;
volatile uint32_t completed_cycles, clock_errors, final_clock, final_pll;
volatile uint32_t restore_failures, restore_fallbacks;
static uint32_t rtc_stamp;
extern void vPortSetupTimerInterrupt(void);
extern void vPortSuppressTicksAndSleep(TickType_t expected_ticks);

void *memset(void *dst, int value, size_t size)
{
    unsigned char *p = dst;
    for (size_t i = 0; i < size; ++i) p[i] = (unsigned char)value;
    return dst;
}
TickType_t xTaskGetTickCount(void) { return stepped_ticks; }
TickType_t xTaskGetTickCountFromISR(void) { return stepped_ticks; }
BaseType_t xTaskGetSchedulerState(void) { return taskSCHEDULER_RUNNING; }
BaseType_t xTaskIncrementTick(void) { return pdFALSE; }
eSleepModeStatus eTaskConfirmSleepModeStatus(void)
{ return scenario == 4 ? eAbortSleep : eStandardSleep; }
void vTaskStepTick(TickType_t ticks) { stepped_ticks += ticks; }
bool AppScreen_IsSleeping(void) { return true; }
void Error_Handler(void) { ++clock_errors; for (;;) {} }
void HAL_SuspendTick(void) { CLEAR_BIT(TIM11->DIER, TIM_DIER_UIE); }
void HAL_NVIC_ClearPendingIRQ(IRQn_Type irq) { (void)irq; }
uint32_t osThreadFlagsSet(osThreadId_t thread, uint32_t flags)
{ (void)thread; return flags; }
HAL_StatusTypeDef HAL_RTC_GetTime(RTC_HandleTypeDef *rtc, RTC_TimeTypeDef *time, uint32_t format)
{
    (void)rtc; (void)format;
    time->Hours = rtc_stamp / (3600U * 256U);
    time->Minutes = (rtc_stamp / (60U * 256U)) % 60U;
    time->Seconds = (rtc_stamp / 256U) % 60U;
    time->SubSeconds = 255U - rtc_stamp % 256U;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_RTC_GetDate(RTC_HandleTypeDef *rtc, RTC_DateTypeDef *date, uint32_t format)
{ (void)rtc; (void)date; (void)format; return HAL_OK; }
HAL_StatusTypeDef HAL_RTCEx_SetWakeUpTimer_IT(RTC_HandleTypeDef *rtc, uint32_t count, uint32_t clock)
{
    (void)count; (void)clock;
    if (scenario == 5) return HAL_ERROR;
    rtc->Instance->CR |= RTC_CR_WUTE | RTC_CR_WUTIE;
    return HAL_OK;
}
void HAL_PWR_EnterSTOPMode(uint32_t regulator, uint8_t entry)
{
    (void)regulator; (void)entry;
    ++stop_calls;
    /* Scenario 0/1: ~90ms RTC; 2: early KEY after ~8ms; 3: STOP ignored. */
    if (scenario != 3) {
        rtc_stamp = (rtc_stamp + (scenario == 2 ? 2U : scenario == 6 ? 35U : 23U)) % (86400U * 256U);
        CLEAR_BIT(RCC->CR, RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY);
        MODIFY_REG(RCC->CFGR, RCC_CFGR_SW | RCC_CFGR_SWS, RCC_CFGR_SWS_HSI);
        if (scenario == 2) EXTI->PR = GPIO_PIN_0;
    }
}
int test_main(void)
{
    rtc_stamp = scenario == 1 ? (86400U * 256U - 10U) : 0U;
    RCC->CFGR = RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL;
    RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY | RCC_CR_HSEON | RCC_CR_HSERDY |
              RCC_CR_PLLON | RCC_CR_PLLRDY;
    RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_HSE | 12U | (96U << RCC_PLLCFGR_PLLN_Pos) |
                   (4U << RCC_PLLCFGR_PLLQ_Pos);
    AppPower_StartRuntimeClock();
    vPortSetupTimerInterrupt();
    uint32_t cycles = scenario == 7 ? 3000U : scenario == 8 ? 3U : 1U;
    for (uint32_t i = 0; i < cycles; ++i) {
        vPortSuppressTicksAndSleep(100U);
        ++completed_cycles;
    }
    final_reload = SysTick->LOAD;
    final_primask = __get_PRIMASK();
    rtc_disarmed = !(RTC->CR & (RTC_CR_WUTE | RTC_CR_WUTIE));
    final_clock = RCC->CFGR;
    final_pll = RCC->PLLCFGR;
    restore_failures = g_app_diagnostics.stop_clock_failures;
    restore_fallbacks = g_app_diagnostics.stop_clock_fallbacks;
    return 0;
}
