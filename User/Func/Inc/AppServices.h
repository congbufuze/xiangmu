#ifndef APP_SERVICES_H
#define APP_SERVICES_H

#include "cmsis_os2.h"
#include "AppTypes.h"
#include "ScreenPower.h"

#define APP_SAVE_REQUEST (1U << 0)
#define APP_KEY_BACK 1U
#define APP_KEY_HOME_OR_SLEEP 2U

extern osMessageQueueId_t KEYQueueHandle;
extern osEventFlagsId_t AppEventHandle;
extern osEventFlagsId_t DataSaveEventHandle;
extern osMutexId_t SensorMutexHandle;
extern osMutexId_t SettingsMutexHandle;

typedef struct {
    uint32_t free_heap;
    uint32_t minimum_free_heap;
    uint32_t key_stack_free;
    uint32_t sensor_stack_free;
    uint32_t ui_stack_free;
    uint32_t save_stack_free;
    uint32_t hardware_stack_free;
    uint32_t key_queue_drops;
    uint32_t save_failures;
    uint32_t saves;
    uint32_t fault;
    const char *fault_task;
    bool settings_loaded;
    ScreenState screen_state;
} AppDiagnostics;

extern volatile AppDiagnostics g_app_diagnostics;

void AppServices_Init(void);
void App_PostEvent(uint32_t events);
uint32_t App_TakeEvents(void);
void AppSettings_Read(AppSettings *settings);
void AppSettings_Write(const AppSettings *settings);
void AppSettings_GetSaveState(AppSettings *settings, uint32_t *generation, uint32_t *changed_at);
void AppSensors_Publish(const AppSensorSnapshot *snapshot);
void AppSensors_Read(AppSensorSnapshot *snapshot);
bool AppScreen_IsSleeping(void);
uint32_t App_MsToTicks(uint32_t milliseconds);
void AppDiagnostics_Update(void);

#endif
