#include "SettingsJournal.h"
#include <string.h>

_Static_assert(sizeof(SettingsRecord) == 32, "Flash record size changed");
_Static_assert(offsetof(SettingsRecord, commit) == 28, "Commit must be the last word");

typedef struct {
    bool found;
    uint32_t bank;
    SettingsRecord record;
    uint32_t free_offset[2];
} ScanResult;

uint32_t SettingsJournal_Crc(const void *data, uint32_t size)
{
    const uint8_t *bytes = data;
    uint32_t crc = 0xFFFFFFFFU;
    for (uint32_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (uint32_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

static bool record_valid(const SettingsRecord *record)
{
    return record->magic == SETTINGS_RECORD_MAGIC &&
           record->commit == SETTINGS_RECORD_COMMIT &&
           AppSettings_Valid(&record->settings) &&
           record->crc == SettingsJournal_Crc(record, offsetof(SettingsRecord, crc));
}

static bool erased(const SettingsRecord *record)
{
    const uint8_t *bytes = (const uint8_t *)record;
    for (uint32_t i = 0; i < sizeof(*record); ++i) {
        if (bytes[i] != 0xFFU) return false;
    }
    return true;
}

static bool scan(const SettingsStorage *storage, ScanResult *result)
{
    memset(result, 0, sizeof(*result));
    result->free_offset[0] = result->free_offset[1] = SETTINGS_BANK_BYTES;
    for (uint32_t bank = 0; bank < 2; ++bank) {
        for (uint32_t offset = 0; offset < SETTINGS_BANK_BYTES; offset += sizeof(SettingsRecord)) {
            SettingsRecord record;
            if (!storage->read(storage->context, bank, offset, &record, sizeof(record))) return false;
            if (erased(&record)) {
                if (result->free_offset[bank] == SETTINGS_BANK_BYTES) result->free_offset[bank] = offset;
            } else if (record_valid(&record) &&
                       (!result->found || (int32_t)(record.sequence - result->record.sequence) > 0)) {
                result->record = record;
                result->bank = bank;
                result->found = true;
            }
        }
    }
    return true;
}

bool SettingsJournal_Load(const SettingsStorage *storage, AppSettings *settings)
{
    ScanResult result;
    if (!scan(storage, &result) || !result.found) return false;
    *settings = result.record.settings;
    return true;
}

bool SettingsJournal_Save(const SettingsStorage *storage, const AppSettings *settings)
{
    ScanResult result;
    if (!AppSettings_Valid(settings) || !scan(storage, &result)) return false;
    if (result.found && memcmp(settings, &result.record.settings, sizeof(*settings)) == 0) return true;
    uint32_t bank = result.found ? result.bank : 0;
    uint32_t offset = result.free_offset[bank];
    if (offset == SETTINGS_BANK_BYTES) {
        /* Keep the bank containing the latest committed record intact. */
        if (result.found) bank ^= 1U;
        if (!storage->erase(storage->context, bank)) return false;
        offset = 0;
    }
    SettingsRecord record;
    memset(&record, 0xFF, sizeof(record));
    record.magic = SETTINGS_RECORD_MAGIC;
    record.sequence = result.found ? result.record.sequence + 1U : 0U;
    record.settings = *settings;
    record.crc = SettingsJournal_Crc(&record, offsetof(SettingsRecord, crc));
    record.commit = SETTINGS_RECORD_COMMIT;
    for (uint32_t i = 0; i < sizeof(record); i += sizeof(uint32_t)) {
        uint32_t word;
        memcpy(&word, (const uint8_t *)&record + i, sizeof(word));
        if (!storage->program_word(storage->context, bank, offset + i, word)) return false;
    }
    SettingsRecord verify;
    return storage->read(storage->context, bank, offset, &verify, sizeof(verify)) &&
           record_valid(&verify) && memcmp(&verify, &record, sizeof(record)) == 0;
}
