/* Runs the real portable application C on a Cortex-M emulator (no HAL/RTOS).
 * Return value is the failing source line; zero means all checks passed. */
#include "AppTypes.h"
#include "ScreenPower.h"
#include "SettingsJournal.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void *memcpy(void *dst, const void *src, size_t n)
{ unsigned char *d = dst; const unsigned char *s = src; while (n--) *d++ = *s++; return dst; }
void *memset(void *dst, int v, size_t n)
{ unsigned char *d = dst; while (n--) *d++ = (unsigned char)v; return dst; }
int memcmp(const void *a, const void *b, size_t n)
{ const unsigned char *x = a, *y = b; while (n--) { if (*x != *y) return *x - *y; ++x; ++y; } return 0; }

volatile uint32_t tests_passed;
#define CHECK(x) do { if (!(x)) return __LINE__; ++tests_passed; } while (0)
static uint8_t banks[2][SETTINGS_BANK_BYTES];
static uint8_t backup[2][SETTINGS_BANK_BYTES];
static int words_left = -1;
static bool fail_erase, fail_read, partial_erase;
static uint32_t programs, erases;
static bool read_bank(void *ctx, uint32_t bank, uint32_t offset, void *dst, uint32_t n)
{
    (void)ctx;
    if (fail_read || bank > 1 || offset + n > SETTINGS_BANK_BYTES) return false;
    memcpy(dst, banks[bank] + offset, n);
    return true;
}
static bool erase_bank(void *ctx, uint32_t bank)
{
    (void)ctx;
    ++erases;
    if (partial_erase) { memset(banks[bank], 0xFF, SETTINGS_BANK_BYTES / 2); return false; }
    if (fail_erase) return false;
    memset(banks[bank], 0xFF, SETTINGS_BANK_BYTES);
    return true;
}
static bool program(void *ctx, uint32_t bank, uint32_t offset, uint32_t value)
{
    (void)ctx;
    if (words_left == 0) return false;
    if (words_left > 0) --words_left;
    uint32_t old;
    memcpy(&old, banks[bank] + offset, 4);
    if ((old & value) != value) return false;
    memcpy(banks[bank] + offset, &value, 4);
    ++programs;
    return true;
}
static const SettingsStorage storage = {0, read_bank, erase_bank, program};

static void fill_bank(unsigned bank, uint32_t first_seq, const AppSettings *settings)
{
    for (unsigned i = 0; i < SETTINGS_BANK_BYTES / sizeof(SettingsRecord); ++i) {
        SettingsRecord record = {0};
        record.magic = SETTINGS_RECORD_MAGIC;
        record.sequence = first_seq + i;
        record.settings = *settings;
        record.crc = SettingsJournal_Crc(&record, offsetof(SettingsRecord, crc));
        record.commit = SETTINGS_RECORD_COMMIT;
        memcpy(banks[bank] + i * sizeof(record), &record, sizeof(record));
    }
}

uint32_t test_main(void)
{
    AppSettings settings, loaded, newer;
    AppSettings_Defaults(&settings);
    CHECK(AppSettings_Valid(&settings));
    newer = settings;
    newer.brightness = 0;
    newer.idle_seconds = 60;
    newer.sleep_seconds = 15;
    AppSettings_Normalize(&newer);
    CHECK(newer.brightness == 5 && newer.sleep_seconds == 65 && AppSettings_Valid(&newer));
    ScreenPower power;
    ScreenPower_Init(&power, 100);
    CHECK(ScreenPower_Step(&power, 10099, 0, &settings) == SCREEN_ACTIVE);
    CHECK(ScreenPower_Step(&power, 10100, 0, &settings) == SCREEN_DIMMED);
    CHECK(ScreenPower_Step(&power, 15099, 0, &settings) == SCREEN_DIMMED);
    CHECK(ScreenPower_Step(&power, 15100, 0, &settings) == SCREEN_SLEEPING);
    CHECK(ScreenPower_Step(&power, 15101, APP_EVENT_ACTIVITY | APP_EVENT_SLEEP_REQUEST, &settings) == SCREEN_ACTIVE);
    CHECK(ScreenPower_Step(&power, 15102, APP_EVENT_SLEEP_REQUEST, &settings) == SCREEN_SLEEPING);
    CHECK(ScreenPower_Step(&power, 15103, APP_EVENT_IDLE_ENTER, &settings) == SCREEN_SLEEPING);
    CHECK(ScreenPower_Step(&power, 15104, APP_EVENT_WAKE_REQUEST, &settings) == SCREEN_ACTIVE);
    CHECK(ScreenPower_Step(&power, 15105, APP_EVENT_IDLE_ENTER, &settings) == SCREEN_DIMMED);
    CHECK(ScreenPower_Step(&power, 15106, APP_EVENT_IDLE_EXIT, &settings) == SCREEN_ACTIVE);
    ScreenPower_Init(&power, UINT32_MAX - 4999U);
    CHECK(ScreenPower_Step(&power, 4999, 0, &settings) == SCREEN_ACTIVE);
    CHECK(ScreenPower_Step(&power, 5000, 0, &settings) == SCREEN_DIMMED);
    CHECK(ScreenPower_Step(&power, 10000, 0, &settings) == SCREEN_SLEEPING);

    CHECK(SettingsJournal_Crc("123456789", 9) == 0xCBF43926U);
    memset(banks, 0xFF, sizeof(banks));
    CHECK(!SettingsJournal_Load(&storage, &loaded));
    CHECK(SettingsJournal_Save(&storage, &settings));
    CHECK(SettingsJournal_Load(&storage, &loaded) && memcmp(&loaded, &settings, sizeof(settings)) == 0);
    uint32_t count = programs;
    CHECK(SettingsJournal_Save(&storage, &settings) && programs == count);
    memcpy(backup, banks, sizeof(banks));
    newer = settings;
    newer.brightness = 75;
    for (int cut = 0; cut < 8; ++cut) {
        memcpy(banks, backup, sizeof(banks));
        words_left = cut;
        CHECK(!SettingsJournal_Save(&storage, &newer));
        CHECK(SettingsJournal_Load(&storage, &loaded) && loaded.brightness == settings.brightness);
        words_left = -1;
        CHECK(SettingsJournal_Save(&storage, &newer));
        CHECK(SettingsJournal_Load(&storage, &loaded) && loaded.brightness == 75);
    }
    /* CRC damage falls back to the previous committed record. */
    memcpy(banks, backup, sizeof(banks));
    CHECK(SettingsJournal_Save(&storage, &newer));
    banks[0][sizeof(SettingsRecord) + offsetof(SettingsRecord, settings) + offsetof(AppSettings, brightness)] ^= 1;
    CHECK(SettingsJournal_Load(&storage, &loaded) && loaded.brightness == 50);
    fail_read = true;
    CHECK(!SettingsJournal_Save(&storage, &newer));
    fail_read = false;
    newer.brightness = 0;
    CHECK(!SettingsJournal_Save(&storage, &newer));
    newer.brightness = 75;

    /* Force rollover with old records in the destination bank. */
    fill_bank(0, 1024, &settings);
    fill_bank(1, 0, &settings);
    memcpy(backup, banks, sizeof(banks));
    fail_erase = true;
    CHECK(!SettingsJournal_Save(&storage, &newer));
    CHECK(SettingsJournal_Load(&storage, &loaded) && loaded.brightness == 50);
    fail_erase = false;
    partial_erase = true;
    CHECK(!SettingsJournal_Save(&storage, &newer));
    CHECK(SettingsJournal_Load(&storage, &loaded) && loaded.brightness == 50);
    partial_erase = false;
    for (int cut = 0; cut < 8; ++cut) {
        memcpy(banks, backup, sizeof(banks));
        words_left = cut;
        CHECK(!SettingsJournal_Save(&storage, &newer));
        CHECK(SettingsJournal_Load(&storage, &loaded) && loaded.brightness == 50);
    }
    words_left = -1;
    CHECK(SettingsJournal_Save(&storage, &newer));
    CHECK(SettingsJournal_Load(&storage, &loaded) && loaded.brightness == 75);
    CHECK(memcmp(banks[0], backup[0], SETTINGS_BANK_BYTES) == 0);
    /* Sequence arithmetic remains valid across uint32 rollover. */
    memset(banks, 0xFF, sizeof(banks));
    fill_bank(0, UINT32_MAX - 511U, &settings);
    CHECK(SettingsJournal_Save(&storage, &newer));
    CHECK(SettingsJournal_Load(&storage, &loaded) && loaded.brightness == 75);
    return 0;
}
