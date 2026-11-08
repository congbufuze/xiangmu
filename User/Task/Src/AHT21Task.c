#include "AppServices.h"
#include "SystemStartup.h"
#include "HWDataAccess.h"
#include "main.h"
#include "WristWake.h"
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
    WristWake wrist = {0};
#endif
    uint32_t last_sample = HAL_GetTick() - 500U;
    for (;;) {
#if HW_USE_IMU
        AppSettings settings;
        AppSettings_Read(&settings);
        if (settings.wrist_enabled && !HWInterface.IMU.ConnectionError) {
            float roll = 0, pitch = 0;
            bool valid = MPU_Read_Angles(&roll, &pitch) == 0;
            if (!valid) ++g_app_diagnostics.imu_read_failures;
            if (WristWake_Update(&wrist, valid, roll, pitch)) {
                App_PostEvent(APP_EVENT_WAKE_REQUEST);
                ++g_app_diagnostics.wrist_wake_count;
            }
        } else wrist = (WristWake){0};
        if (!HWInterface.IMU.ConnectionError) {
            /* Drain DMP output even while the screen sleeps, preventing FIFO overflow. */
            short gyro[3], accel[3], sensors;
            long quat[4];
            unsigned long timestamp;
            unsigned char more = 0;
            for (uint8_t n = 0; n < 16; ++n) {
                if (dmp_read_fifo(gyro, accel, quat, &timestamp, &sensors, &more) || !more) break;
            }
        }
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
                if (AHT_Read(&humidity, &temperature) == 0 &&
                    temperature >= -40.0f && temperature <= 85.0f && humidity >= 0 && humidity <= 100) {
                    snapshot.humidity = humidity;
                    snapshot.temperature = temperature;
                    snapshot.valid |= APP_SENSOR_ENV_VALID;
                } else {
                    snapshot.valid &= (uint8_t)~APP_SENSOR_ENV_VALID;
                    ++g_app_diagnostics.aht_read_failures;
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
