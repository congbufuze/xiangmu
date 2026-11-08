#include "ScreenPower.h"

void ScreenPower_Init(ScreenPower *power, uint32_t now)
{
    power->state = SCREEN_ACTIVE;
    power->last_activity_ms = now;
}

ScreenState ScreenPower_Step(ScreenPower *power, uint32_t now,
                             uint32_t events, const AppSettings *settings)
{
    /* Fresh input wins over a timeout or a concurrent sleep request. */
    if (events & (APP_EVENT_ACTIVITY | APP_EVENT_IDLE_EXIT | APP_EVENT_WAKE_REQUEST)) {
        power->last_activity_ms = now;
        power->state = SCREEN_ACTIVE;
    } else if ((events & APP_EVENT_SLEEP_REQUEST) ||
               (uint32_t)(now - power->last_activity_ms) >= settings->sleep_seconds * 1000U) {
        power->state = SCREEN_SLEEPING;
    } else if (power->state != SCREEN_SLEEPING &&
               ((events & APP_EVENT_IDLE_ENTER) ||
                (uint32_t)(now - power->last_activity_ms) >= settings->idle_seconds * 1000U)) {
        power->state = SCREEN_DIMMED;
    }
    return power->state;
}
