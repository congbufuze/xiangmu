#ifndef LOW_POWER_POLICY_H
#define LOW_POWER_POLICY_H
#include <stdbool.h>
#include <stdint.h>
#define APP_ENV_ACTIVE_MS 500U
#define APP_ENV_SLEEP_MS 5000U
#define APP_IMU_SERVICE_MS 100U
/* UINT32_MAX means no dirty settings and no polling deadline. */
static inline uint32_t AppSave_WaitMs(uint32_t now, uint32_t generation,
    uint32_t saved, uint32_t changed_at, bool retry, uint32_t last_attempt)
{
    if (generation == saved) return UINT32_MAX;
    uint32_t age = now - changed_at;
    uint32_t debounce = age < 1000U ? 1000U - age : 0U;
    uint32_t elapsed = now - last_attempt;
    uint32_t backoff = retry && elapsed < 5000U ? 5000U - elapsed : 0U;
    return debounce > backoff ? debounce : backoff;
}
static inline uint32_t AppEnv_IntervalMs(bool sleeping)
{
    return sleeping ? APP_ENV_SLEEP_MS : APP_ENV_ACTIVE_MS;
}
#endif
