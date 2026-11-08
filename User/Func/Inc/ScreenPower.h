#ifndef SCREEN_POWER_H
#define SCREEN_POWER_H

#include "AppTypes.h"

#define APP_EVENT_ACTIVITY       (1U << 0)
#define APP_EVENT_IDLE_ENTER     (1U << 1)
#define APP_EVENT_IDLE_EXIT      (1U << 2)
#define APP_EVENT_SLEEP_REQUEST  (1U << 3)
#define APP_EVENT_WAKE_REQUEST   (1U << 4)
#define APP_EVENT_SENSOR_UPDATED (1U << 5)
#define APP_EVENT_SETTINGS       (1U << 6)
#define APP_EVENT_WRIST_DOWN     (1U << 7)
#define APP_EVENT_ALL            0xFFU

typedef enum { SCREEN_ACTIVE, SCREEN_DIMMED, SCREEN_SLEEPING } ScreenState;
typedef struct {
    ScreenState state;
    uint32_t last_activity_ms;
} ScreenPower;

void ScreenPower_Init(ScreenPower *power, uint32_t now);
ScreenState ScreenPower_Step(ScreenPower *power, uint32_t now,
                             uint32_t events, const AppSettings *settings);

#endif
