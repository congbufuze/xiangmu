#include "AppServices.h"
#include "AppPower.h"
#include "SettingsFlash.h"
#include "HWDataAccess.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

static AppSettings s_settings;
static AppSensorSnapshot s_sensors;
static uint32_t s_generation;
static uint32_t s_changed_at;
volatile AppDiagnostics g_app_diagnostics;

extern osThreadId_t KeyTaskHandle, AHT21TaskHandle, UITaskHandle, DataSaveTaskHandle;

uint32_t App_MsToTicks(uint32_t milliseconds)
{
    uint64_t ticks = ((uint64_t)milliseconds * osKernelGetTickFreq() + 999U) / 1000U;
    return ticks == 0 ? 1U : (uint32_t)ticks;
}

void AppServices_Init(void)
{
    AppSettings_Defaults(&s_settings);
    g_app_diagnostics.settings_loaded = SettingsFlash_Load(&s_settings);
    s_sensors.temperature = HWInterface.AHT21.temperature;
    s_sensors.humidity = HWInterface.AHT21.humidity;
    s_sensors.steps = HWInterface.IMU.Steps;
    s_sensors.battery = HWInterface.Power.power_remain;
    s_sensors.heart_rate = HWInterface.HR_meter.HrRate;
    s_sensors.spo2 = HWInterface.HR_meter.SPO2;
    s_sensors.direction = HWInterface.Ecompass.direction;
    s_sensors.altitude = HWInterface.Barometer.altitude;
}

void App_PostEvent(uint32_t events)
{
    if (osEventFlagsSet(AppEventHandle, events) & osFlagsError) Error_Handler();
    AppPower_NotifyUI();
}

uint32_t App_TakeEvents(void)
{
    uint32_t events = osEventFlagsWait(AppEventHandle, APP_EVENT_ALL, osFlagsWaitAny, 0);
    if (events == osFlagsErrorResource || events == osFlagsErrorTimeout) return 0;
    if (events & osFlagsError) Error_Handler();
    return events;
}

void AppSettings_Read(AppSettings *settings)
{
    if (osMutexAcquire(SettingsMutexHandle, osWaitForever) != osOK) Error_Handler();
    *settings = s_settings;
    if (osMutexRelease(SettingsMutexHandle) != osOK) Error_Handler();
}

void AppSettings_Write(const AppSettings *settings)
{
    AppSettings normalized = *settings;
    AppSettings_Normalize(&normalized);
    if (osMutexAcquire(SettingsMutexHandle, osWaitForever) != osOK) Error_Handler();
    bool changed = memcmp(&normalized, &s_settings, sizeof(normalized)) != 0;
    if (changed) {
        s_settings = normalized;
        ++s_generation;
        s_changed_at = HAL_GetTick();
    }
    if (osMutexRelease(SettingsMutexHandle) != osOK) Error_Handler();
    /* Also resync widgets when normalization yields the existing value
     * (e.g. selecting a sleep timeout earlier than the dim timeout). */
    App_PostEvent(APP_EVENT_SETTINGS | APP_EVENT_ACTIVITY);
    if (changed) {
        if (osEventFlagsSet(DataSaveEventHandle, APP_SAVE_REQUEST) & osFlagsError) Error_Handler();
    }
}

void AppSettings_GetSaveState(AppSettings *settings, uint32_t *generation, uint32_t *changed_at)
{
    if (osMutexAcquire(SettingsMutexHandle, osWaitForever) != osOK) Error_Handler();
    *settings = s_settings;
    *generation = s_generation;
    *changed_at = s_changed_at;
    if (osMutexRelease(SettingsMutexHandle) != osOK) Error_Handler();
}

void AppSensors_Publish(const AppSensorSnapshot *snapshot)
{
    if (osMutexAcquire(SensorMutexHandle, osWaitForever) != osOK) Error_Handler();
    s_sensors = *snapshot;
    if (osMutexRelease(SensorMutexHandle) != osOK) Error_Handler();
    /* The UI reads the latest snapshot unconditionally on wake. */
    if (!AppScreen_IsSleeping()) App_PostEvent(APP_EVENT_SENSOR_UPDATED);
}

void AppSensors_Read(AppSensorSnapshot *snapshot)
{
    if (osMutexAcquire(SensorMutexHandle, osWaitForever) != osOK) Error_Handler();
    *snapshot = s_sensors;
    if (osMutexRelease(SensorMutexHandle) != osOK) Error_Handler();
}

bool AppScreen_IsSleeping(void)
{
    return g_app_diagnostics.screen_state == SCREEN_SLEEPING;
}

void AppDiagnostics_Update(void)
{
    g_app_diagnostics.free_heap = xPortGetFreeHeapSize();
    g_app_diagnostics.minimum_free_heap = xPortGetMinimumEverFreeHeapSize();
    g_app_diagnostics.key_stack_free = osThreadGetStackSpace(KeyTaskHandle);
    g_app_diagnostics.sensor_stack_free = osThreadGetStackSpace(AHT21TaskHandle);
    g_app_diagnostics.ui_stack_free = osThreadGetStackSpace(UITaskHandle);
    g_app_diagnostics.save_stack_free = osThreadGetStackSpace(DataSaveTaskHandle);
}
