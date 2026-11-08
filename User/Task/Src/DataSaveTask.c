#include "AppServices.h"
#include "SettingsFlash.h"
#include "SystemStartup.h"
#include "main.h"

void StartDataSaveTask(void *argument)
{
    (void)argument;
    SystemStartup_WaitReady();
    uint32_t saved_generation = 0;
    uint32_t last_attempt = HAL_GetTick() - 5000U;
    bool retry = false;
    for (;;) {
        uint32_t event = osEventFlagsWait(DataSaveEventHandle, APP_SAVE_REQUEST,
                                          osFlagsWaitAny, App_MsToTicks(250));
        if ((event & osFlagsError) && event != osFlagsErrorTimeout && event != osFlagsErrorResource) {
            Error_Handler();
        }
        AppSettings settings;
        uint32_t generation, changed_at;
        AppSettings_GetSaveState(&settings, &generation, &changed_at);
        uint32_t now = HAL_GetTick();
        if (generation == saved_generation || (uint32_t)(now - changed_at) < 1000U ||
            (retry && (uint32_t)(now - last_attempt) < 5000U)) continue;
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
    }
}
