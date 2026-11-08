#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#define APP_SETTINGS_VERSION 1U

typedef struct {
    uint32_t version;
    uint16_t idle_seconds;
    uint16_t sleep_seconds;
    uint8_t brightness;
    uint8_t wrist_enabled;
    uint8_t reserved[6];
} AppSettings;

typedef struct {
    float temperature;
    float humidity;
    uint32_t sampled_at_ms;
    uint32_t sequence;
    uint16_t steps;
    uint16_t direction;
    uint16_t altitude;
    uint8_t battery;
    uint8_t heart_rate;
    uint8_t spo2;
    uint8_t valid;
} AppSensorSnapshot;

#define APP_SENSOR_ENV_VALID (1U << 0)
#define APP_SENSOR_IMU_VALID (1U << 1)

void AppSettings_Defaults(AppSettings *settings);
bool AppSettings_Valid(const AppSettings *settings);
void AppSettings_Normalize(AppSettings *settings);

#endif
