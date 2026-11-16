/* Actual AppPower.c with HAL/kernel service boundaries stubbed. */
#include "AppPower.h"
#include "AppServices.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "key.h"
#include "CST816.h"
#include "rtc.h"

uint32_t SystemCoreClock = 100000000U;
volatile uint32_t uwTick;
TIM_HandleTypeDef htim11 = {.Instance = TIM11};
RTC_HandleTypeDef hrtc;
osThreadId_t KeyTaskHandle, UITaskHandle;
volatile AppDiagnostics g_app_diagnostics;
static uint32_t ticks, suspended, notifications, notified_flags;
static osThreadId_t notified_task;
void *memset(void *dst, int value, size_t size)
{
    unsigned char *p = dst;
    for (size_t i = 0; i < size; ++i) p[i] = (unsigned char)value;
    return dst;
}
TickType_t xTaskGetTickCount(void) { return ticks; }
TickType_t xTaskGetTickCountFromISR(void) { return ticks; }
BaseType_t xTaskGetSchedulerState(void) { return taskSCHEDULER_RUNNING; }
void xPortSysTickHandler(void) { ++ticks; }
void HAL_SuspendTick(void) { ++suspended; CLEAR_BIT(TIM11->DIER, TIM_DIER_UIE); }
void HAL_NVIC_ClearPendingIRQ(IRQn_Type irq) { (void)irq; }
uint32_t osThreadFlagsSet(osThreadId_t task, uint32_t flags)
{ ++notifications; notified_task = task; notified_flags = flags; return flags; }
bool AppScreen_IsSleeping(void) { return false; }
HAL_StatusTypeDef HAL_RTC_GetTime(RTC_HandleTypeDef *rtc, RTC_TimeTypeDef *time, uint32_t format)
{ (void)rtc; (void)format; *time = (RTC_TimeTypeDef){0}; return HAL_OK; }
HAL_StatusTypeDef HAL_RTC_GetDate(RTC_HandleTypeDef *rtc, RTC_DateTypeDef *date, uint32_t format)
{ (void)rtc; (void)format; *date = (RTC_DateTypeDef){0}; return HAL_OK; }
HAL_StatusTypeDef HAL_RTCEx_SetWakeUpTimer_IT(RTC_HandleTypeDef *rtc, uint32_t count, uint32_t clock)
{ (void)rtc; (void)count; (void)clock; return HAL_OK; }
void HAL_PWR_EnterSTOPMode(uint32_t regulator, uint8_t entry)
{ (void)regulator; (void)entry; }
void SystemClock_Config(void) {}
void Error_Handler(void) { for (;;) {} }

volatile uint32_t tests_passed;
#define CHECK(x) do { if (!(x)) return __LINE__; ++tests_passed; } while (0)
extern void SysTick_Handler(void);
int test_main(void)
{
    uwTick = 1234; ticks = 100;
    TIM11->CR1 = TIM_CR1_CEN; TIM11->DIER = TIM_DIER_UIE;
    TIM11->SR = TIM_SR_UIF;
    CHECK(HAL_GetTick() == 1234);
    AppPower_StartRuntimeClock();
    CHECK(HAL_GetTick() == 1235);
    CHECK(g_app_diagnostics.runtime_tick_active);
    CHECK(!(TIM11->CR1 & TIM_CR1_CEN));
    CHECK(!(TIM11->DIER & TIM_DIER_UIE));
    CHECK(suspended == 1);
    ticks += 167; uwTick = 0;
    CHECK(HAL_GetTick() == 1402);
    AppPower_StartRuntimeClock();
    CHECK(suspended == 1);
    CHECK(HAL_GetTick() == 1402);
    ticks = UINT32_MAX;
    CHECK(HAL_GetTick() == 1134);
    ticks = 0;
    CHECK(HAL_GetTick() == 1135);
    SysTick_Handler();
    CHECK(HAL_GetTick() == 1136);

    AppPower_NotifyUI(); CHECK(notifications == 0);
    UITaskHandle = (osThreadId_t)0x1234;
    AppPower_NotifyUI();
    CHECK(notifications == 1 && notified_task == UITaskHandle && notified_flags == APP_UI_NOTIFY);
    HAL_GPIO_EXTI_Callback(KEY1_PIN);
    CHECK(notifications == 1);
    KeyTaskHandle = (osThreadId_t)0x5678;
    HAL_GPIO_EXTI_Callback(KEY1_PIN);
    CHECK(notified_task == KeyTaskHandle && notified_flags == APP_KEY_EDGE);
    CHECK(g_app_diagnostics.key_irq_count == 1);
    HAL_GPIO_EXTI_Callback(TOUCH_INT_PIN);
    CHECK(notified_task == UITaskHandle && notified_flags == APP_UI_TOUCH_IRQ);
    CHECK(g_app_diagnostics.touch_irq_count == 1);
    HAL_GPIO_EXTI_Callback(GPIO_PIN_2);
    CHECK(notifications == 3);
    SCB->SCR = SCB_SCR_SLEEPDEEP_Msk;
    SysTick->LOAD = 999999;
    PreSleepProcessing(0);
    CHECK(!(SCB->SCR & SCB_SCR_SLEEPDEEP_Msk));
    CHECK(g_app_diagnostics.tickless_entries == 1);
    CHECK(g_app_diagnostics.tickless_last_programmed_ticks == 10);
    PostSleepProcessing(10);
    CHECK(g_app_diagnostics.tickless_exits == 1);
    return 0;
}
