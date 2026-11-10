#include "AppServices.h"
#include "SettingsFlash.h"
#include "SystemStartup.h"
#include "main.h"
#include "LowPowerPolicy.h"

void StartDataSaveTask(void *argument)
{
    (void)argument;
    SystemStartup_WaitReady();
    uint32_t saved_generation = 0;
    uint32_t last_attempt = HAL_GetTick() - 5000U;
    bool retry = false;
    uint32_t wait_ticks = osWaitForever;
    for (;;) {
        uint32_t event = osEventFlagsWait(DataSaveEventHandle, APP_SAVE_REQUEST,
                                          osFlagsWaitAny, wait_ticks);
        if ((event & osFlagsError) && event != osFlagsErrorTimeout && event != osFlagsErrorResource) {
            Error_Handler();
        }
        AppSettings settings;
        uint32_t generation, changed_at;
        AppSettings_GetSaveState(&settings, &generation, &changed_at);
        uint32_t now = HAL_GetTick();
        uint32_t wait_ms = AppSave_WaitMs(now, generation, saved_generation,
                                        changed_at, retry, last_attempt);
        wait_ticks = wait_ms == UINT32_MAX ? osWaitForever : App_MsToTicks(wait_ms);
        if (wait_ms != 0U) continue;
        last_attempt = now;
        /* No settings mutex is held while Flash is being programmed/erased. */
        if (SettingsFlash_Save(&settings)) {
            saved_generation = generation;
            retry = false;
            ++g_app_diagnostics.saves;
        } else {
            retry = true;
            ++g_app_diagnostics.save_failures;
        }
        /* A change during Flash programming remains dirty and gets a deadline. */
        AppSettings_GetSaveState(&settings, &generation, &changed_at);
        wait_ms = AppSave_WaitMs(HAL_GetTick(), generation, saved_generation,
                                changed_at, retry, last_attempt);
        wait_ticks = wait_ms == UINT32_MAX ? osWaitForever : App_MsToTicks(wait_ms);
    }
}
