#include "AppPower.h"
#include "AppServices.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "key.h"
#include "CST816.h"
#include "rtc.h"

extern TIM_HandleTypeDef htim11;
extern osThreadId_t KeyTaskHandle, UITaskHandle;
static volatile uint32_t runtime_clock;
static uint32_t tick_offset;
static bool stop_clock_degraded;
_Static_assert(configTICK_RATE_HZ == 1000, "Runtime HAL clock requires 1ms RTOS ticks");

#define APP_STOP_MIN_IDLE_TICKS 25U
#define APP_STOP_GUARD_MS 10U
#define APP_STOP_MAX_MS 90U
#define APP_RTC_LSI_HZ 32000U

/* STOP retains PLLCFGR, bus dividers and Flash latency, but switches SYSCLK
 * to HSI. Do not rerun startup SystemClock_Config(): its 2ms PLL timeout
 * cannot be measured with the RTC's 3.9ms calendar resolution. It also
 * needlessly reinitializes TIM11 inside the idle task with IRQs masked. */
static bool AppPower_WaitClock(uint32_t mask, uint32_t value, uint32_t timeout_ms,
                               bool sysclk)
{
    const uint32_t limit = (HSI_VALUE / 1000U) * timeout_ms;
    uint32_t start = DWT->CYCCNT;
    uint32_t budget = limit;
    do {
        uint32_t actual = sysclk ? RCC->CFGR : RCC->CR;
        if ((actual & mask) == value) return true;
        /* The iteration budget also bounds the wait if a debugger disables
         * CYCCNT. No interrupt, RTOS tick or RTC calendar is needed here. */
        if ((uint32_t)(DWT->CYCCNT - start) >= limit) break;
    } while (--budget);
    return false;
}

static bool AppPower_RestoreClock(void)
{
    uint32_t saved_demcr = CoreDebug->DEMCR;
    uint32_t saved_dwt = DWT->CTRL;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    bool ok = true;
    uint32_t stage = 1U;
    if (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC_HSE) {
        SET_BIT(RCC->CR, RCC_CR_HSEON);
        ok = AppPower_WaitClock(RCC_CR_HSERDY, RCC_CR_HSERDY, 100U, false);
    }
    if (ok) {
        stage = 2U;
        SET_BIT(RCC->CR, RCC_CR_PLLON);
        ok = AppPower_WaitClock(RCC_CR_PLLRDY, RCC_CR_PLLRDY, 10U, false);
    }
    if (ok) {
        stage = 3U;
        MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_CFGR_SW_PLL);
        ok = AppPower_WaitClock(RCC_CFGR_SWS, RCC_CFGR_SWS_PLL, 10U, true);
    }
    if (!ok) {
        ++g_app_diagnostics.stop_clock_failures;
        g_app_diagnostics.stop_clock_error = stage;
        /* Recover the same 100MHz from HSI if the external oscillator fails.
         * Keep peripheral divisors and the port's counts-per-tick unchanged.
         * Stay in ordinary Sleep afterward, until the hardware is checked. */
        stop_clock_degraded = true;
        MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_CFGR_SW_HSI);
        ok = AppPower_WaitClock(RCC_CFGR_SWS, RCC_CFGR_SWS_HSI, 10U, true);
        if (ok) {
            CLEAR_BIT(RCC->CR, RCC_CR_PLLON);
            ok = AppPower_WaitClock(RCC_CR_PLLRDY, 0U, 10U, false);
        }
        if (ok) {
            /* HSI/16 = 1MHz, VCO = 200MHz, PLLP=2, SYSCLK = 100MHz.
             * This project uses no USB/SDIO requiring a 48MHz PLLQ clock. */
            if (SystemCoreClock != 100000000U || HSI_VALUE != 16000000U) ok = false;
            else {
                RCC->PLLCFGR = 16U | (200U << RCC_PLLCFGR_PLLN_Pos) |
                               (4U << RCC_PLLCFGR_PLLQ_Pos);
                SET_BIT(RCC->CR, RCC_CR_PLLON);
                ok = AppPower_WaitClock(RCC_CR_PLLRDY, RCC_CR_PLLRDY, 10U, false);
            }
        }
        if (ok) {
            MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_CFGR_SW_PLL);
            ok = AppPower_WaitClock(RCC_CFGR_SWS, RCC_CFGR_SWS_PLL, 10U, true);
        }
        if (ok) ++g_app_diagnostics.stop_clock_fallbacks;
    }
    DWT->CTRL = saved_dwt;
    CoreDebug->DEMCR = saved_demcr;
    __DSB();
    __ISB();
    return ok;
}

static uint32_t AppPower_RtcStamp(void)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    if (HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK ||
        HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK) return 0U;
    return (((uint32_t)time.Hours * 3600U + (uint32_t)time.Minutes * 60U +
             time.Seconds) * 256U) + (255U - time.SubSeconds);
}

static uint32_t AppPower_RtcElapsedSubseconds(uint32_t start, uint32_t now)
{
    const uint32_t day = 86400U * 256U;
    return now >= start ? now - start : day - start + now;
}

static uint32_t AppPower_RtcElapsedMs(uint32_t start, uint32_t now)
{
    return (AppPower_RtcElapsedSubseconds(start, now) * 1000U + 128U) / 256U;
}

static void AppPower_DisarmRtcWakeup(void)
{
    __HAL_RTC_WRITEPROTECTION_DISABLE(&hrtc);
    __HAL_RTC_WAKEUPTIMER_DISABLE(&hrtc);
    __HAL_RTC_WAKEUPTIMER_DISABLE_IT(&hrtc, RTC_IT_WUT);
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
    __HAL_RTC_WRITEPROTECTION_ENABLE(&hrtc);
    __HAL_RTC_WAKEUPTIMER_EXTI_CLEAR_FLAG();
    HAL_NVIC_ClearPendingIRQ(RTC_WKUP_IRQn);
}

/* TIM11 remains the CubeMX HAL timebase during startup. Once the RTOS and
 * hardware are ready, the compensated RTOS tick replaces the redundant 1kHz IRQ.
 * During STOP, RTC time supplies elapsed duration to the existing tickless port.
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
    ++g_app_diagnostics.tickless_entries;
    g_app_diagnostics.tickless_last_programmed_ticks =
        (SysTick->LOAD + 1U) / (SystemCoreClock / configTICK_RATE_HZ);

    if (runtime_clock && !stop_clock_degraded && AppScreen_IsSleeping() &&
        expected_ticks >= APP_STOP_MIN_IDLE_TICKS) {
        uint32_t sleep_ms = expected_ticks - APP_STOP_GUARD_MS;
        if (sleep_ms > APP_STOP_MAX_MS) sleep_ms = APP_STOP_MAX_MS;
        uint32_t wut_count = ((sleep_ms * (APP_RTC_LSI_HZ / 16U)) + 999U) / 1000U;
        if (wut_count) --wut_count;
        uint32_t rtc_before = AppPower_RtcStamp();
        if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, wut_count,
                                        RTC_WAKEUPCLOCK_RTCCLK_DIV16) == HAL_OK) {
            uint32_t tick_counts = SystemCoreClock / configTICK_RATE_HZ;
            uint32_t max_counts = expected_ticks * tick_counts;
            uint32_t port_reload = SysTick->LOAD;
            uint32_t port_remaining = SysTick->VAL;
            uint32_t phase_counts = port_remaining < max_counts ?
                max_counts - port_remaining : 0U;

            SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
            __DSB();
            HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
            SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

            /* An already-pending EXTI can make STM32F4 ignore STOP entry.
             * STOP wake restores HSI as SYSCLK; otherwise this was a spurious
             * WFI return and PLL/SysTick must remain untouched. */
            if (__HAL_RCC_GET_SYSCLK_SOURCE() != RCC_CFGR_SWS_HSI) {
                AppPower_DisarmRtcWakeup();
                ++g_app_diagnostics.stop_entry_aborts;
                __DSB();
                __ISB();
                return;
            }

            ++g_app_diagnostics.stop_entries;
            g_app_diagnostics.stop_wake_exti_latched |= EXTI->PR &
                ((uint32_t)KEY1_PIN | (uint32_t)TOUCH_INT_PIN);

            /* STOP resumes on HSI. Freeze SysTick until the PLL is restored. */
            SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk;
            if (!AppPower_RestoreClock()) Error_Handler();

            uint32_t rtc_after = AppPower_RtcStamp();
            uint32_t elapsed_rtc = AppPower_RtcElapsedSubseconds(rtc_before, rtc_after);
            /* RTC units are 1/256 second, whereas tick_counts is cycles per
             * millisecond. Convert with cycles per SECOND, using 64 bits to
             * avoid overflow. Also preserve the partly elapsed first tick. */
            uint32_t elapsed_counts = (uint32_t)
                (((uint64_t)elapsed_rtc * SystemCoreClock + 128U) / 256U) + phase_counts;
            g_app_diagnostics.stop_last_elapsed_ms =
                AppPower_RtcElapsedMs(rtc_before, rtc_after);
            /* vTaskStepTick may not cross the next task deadline. Leave its
             * final tick for the normal ISR, including a delayed PLL restart.
             * A one-count reload would also keep VAL at zero forever. */
            if (elapsed_counts > max_counts - tick_counts)
                elapsed_counts = max_counts - tick_counts;
            uint32_t remaining_counts = max_counts - elapsed_counts;

            AppPower_DisarmRtcWakeup();
            /* Match the FreeRTOS port's elapsed-decrement formula, which uses
             * expected_ticks * counts_per_tick as its reference interval. */
            SysTick->LOAD = remaining_counts - 1U;
            SysTick->VAL = 0U;
            SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                            SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
            /* VAL stays zero until the first clock reloads the counter. The
             * port reads VAL immediately after this hook: wait for that load,
             * then restore its original periodic reload for COUNTFLAG maths. */
            while (SysTick->VAL == 0U) { }
            SysTick->LOAD = port_reload;
            ++g_app_diagnostics.stop_exits;
            __DSB();
            __ISB();
            return;
        }
    }

    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
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
