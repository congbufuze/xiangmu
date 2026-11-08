#include "AppTypes.h"
#include <string.h>

static bool contains(const uint16_t *values, uint32_t count, uint16_t value)
{
    for (uint32_t i = 0; i < count; ++i) {
        if (values[i] == value) return true;
    }
    return false;
}

static const uint16_t idle_options[] = {10, 15, 20, 30, 45, 60};
static const uint16_t sleep_options[] = {15, 20, 25, 35, 50, 65};

void AppSettings_Defaults(AppSettings *settings)
{
    memset(settings, 0, sizeof(*settings));
    settings->version = APP_SETTINGS_VERSION;
    settings->idle_seconds = 10;
    settings->sleep_seconds = 15;
    settings->brightness = 50;
}

bool AppSettings_Valid(const AppSettings *settings)
{
    return settings->version == APP_SETTINGS_VERSION &&
           settings->brightness >= 5 && settings->brightness <= 100 &&
           settings->wrist_enabled <= 1 &&
           contains(idle_options, 6, settings->idle_seconds) &&
           contains(sleep_options, 6, settings->sleep_seconds) &&
           settings->sleep_seconds > settings->idle_seconds;
}

void AppSettings_Normalize(AppSettings *settings)
{
    settings->version = APP_SETTINGS_VERSION;
    if (settings->brightness < 5) settings->brightness = 5;
    if (settings->brightness > 100) settings->brightness = 100;
    settings->wrist_enabled = settings->wrist_enabled ? 1 : 0;
    if (!contains(idle_options, 6, settings->idle_seconds)) settings->idle_seconds = 10;
    if (!contains(sleep_options, 6, settings->sleep_seconds)) settings->sleep_seconds = 15;
    if (settings->sleep_seconds <= settings->idle_seconds) {
        settings->sleep_seconds = settings->idle_seconds + 5;
    }
    memset(settings->reserved, 0, sizeof(settings->reserved));
}
