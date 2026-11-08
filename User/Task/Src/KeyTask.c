#include "AppServices.h"
#include "SystemStartup.h"
#include "key.h"
void StartKeyTask(void *argument)
{
    (void)argument;
    SystemStartup_WaitReady();
    for (;;) {
        uint8_t command = KeyScan(0);
        if (command == APP_KEY_BACK || command == APP_KEY_HOME_OR_SLEEP) {
            if (osMessageQueuePut(KEYQueueHandle, &command, 0, 0) != osOK) {
                ++g_app_diagnostics.key_queue_drops;
                App_PostEvent(APP_EVENT_ACTIVITY);
            }
        }
        osDelay(App_MsToTicks(5));
    }
}
