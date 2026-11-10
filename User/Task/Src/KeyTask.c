#include "AppServices.h"
#include "AppPower.h"
#include "SystemStartup.h"
#include "key.h"
void StartKeyTask(void *argument)
{
    (void)argument;
    SystemStartup_WaitReady();
    bool held = false;
    for (;;) {
        /* Both edges are configured in CubeMX. Debounce in task context. */
        osDelay(App_MsToTicks(15));
        bool pressed = KEY1 == GPIO_PIN_RESET;
        if (pressed && !held) {
            if (AppScreen_IsSleeping()) App_PostEvent(APP_EVENT_WAKE_REQUEST);
            else {
                uint8_t command = APP_KEY_BACK;
                if (osMessageQueuePut(KEYQueueHandle, &command, 0, 0) != osOK)
                    ++g_app_diagnostics.key_queue_drops;
                App_PostEvent(APP_EVENT_ACTIVITY);
            }
        }
        held = pressed;
        (void)osThreadFlagsWait(APP_KEY_EDGE, osFlagsWaitAny, osWaitForever);
    }
}
