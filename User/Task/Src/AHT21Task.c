#include "AppServices.h"
#include "SystemStartup.h"
#include "HWDataAccess.h"
#include "main.h"
#if HW_USE_IMU
#include "mpu6050.h"
#endif
void StartAHT21Task(void *argument)
{
    (void)argument;
    SystemStartup_WaitReady();
    AppSensorSnapshot snapshot;
    AppSensors_Read(&snapshot);
#if HW_USE_IMU
    bool was_up = false;
#endif
    uint32_t last_sample = HAL_GetTick() - 500U;
    for (;;) {
#if HW_USE_IMU
        AppSettings settings;
        AppSettings_Read(&settings);
        if (settings.wrist_enabled && !HWInterface.IMU.ConnectionError) {
            bool up = MPU_isHorizontal();
            if (up != was_up) App_PostEvent(up ? APP_EVENT_WAKE_REQUEST : APP_EVENT_WRIST_DOWN);
            was_up = up;
        } else was_up = false;
#endif
        uint32_t now = HAL_GetTick();
        if ((uint32_t)(now - last_sample) >= 500U) {
            last_sample = now;
#if HW_USE_BAT
            snapshot.battery = HWInterface.Power.BatCalculate();
#endif
#if HW_USE_IMU
            if (!HWInterface.IMU.ConnectionError) {
                snapshot.steps = HWInterface.IMU.GetSteps();
                snapshot.valid |= APP_SENSOR_IMU_VALID;
            }
#endif
#if HW_USE_AHT21
            if (!HWInterface.AHT21.ConnectionError) {
                float humidity, temperature;
                HWInterface.AHT21.GetHumiTemp(&humidity, &temperature);
                if (temperature > -10.0f && temperature < 50.0f && humidity >= 0 && humidity <= 100) {
                    snapshot.humidity = humidity;
                    snapshot.temperature = temperature;
                    snapshot.valid |= APP_SENSOR_ENV_VALID;
                }
            }
#endif
            snapshot.sampled_at_ms = now;
            ++snapshot.sequence;
            AppSensors_Publish(&snapshot);
        }
        osDelay(App_MsToTicks(100));
    }
}
