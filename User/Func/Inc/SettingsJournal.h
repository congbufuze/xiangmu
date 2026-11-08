#ifndef SETTINGS_JOURNAL_H
#define SETTINGS_JOURNAL_H

#include "AppTypes.h"
#include <stddef.h>

#define SETTINGS_BANK_BYTES 16384U
#define SETTINGS_RECORD_MAGIC 0x57415443U
#define SETTINGS_RECORD_COMMIT 0xC0DEC0DEU

typedef struct {
    uint32_t magic;
    uint32_t sequence;
    AppSettings settings;
    uint32_t crc;
    uint32_t commit;
} SettingsRecord;

/* Backend uses offsets within one of the two dedicated banks. */
typedef struct {
    void *context;
    bool (*read)(void *, uint32_t bank, uint32_t offset, void *, uint32_t size);
    bool (*erase)(void *, uint32_t bank);
    bool (*program_word)(void *, uint32_t bank, uint32_t offset, uint32_t word);
} SettingsStorage;

bool SettingsJournal_Load(const SettingsStorage *storage, AppSettings *settings);
bool SettingsJournal_Save(const SettingsStorage *storage, const AppSettings *settings);
uint32_t SettingsJournal_Crc(const void *data, uint32_t size);

#endif
