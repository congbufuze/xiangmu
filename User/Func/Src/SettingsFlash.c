#include "SettingsFlash.h"
#include "SettingsJournal.h"
#include "main.h"
#include "iwdg.h"
#include <string.h>

/* Link fails if the dedicated regions are absent from the linker script. */
extern const uint8_t __settings_a_start__, __settings_b_start__;

static uint32_t bank_address(uint32_t bank)
{
    return (uint32_t)(bank == 0 ? &__settings_a_start__ : &__settings_b_start__);
}

static bool read_bank(void *context, uint32_t bank, uint32_t offset, void *data, uint32_t size)
{
    (void)context;
    if (bank > 1 || offset > SETTINGS_BANK_BYTES || size > SETTINGS_BANK_BYTES - offset) return false;
    memcpy(data, (const void *)(bank_address(bank) + offset), size);
    return true;
}

static bool erase_bank(void *context, uint32_t bank)
{
    (void)context;
    if (bank > 1) return false;
    FLASH_EraseInitTypeDef erase = {0};
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = bank == 0 ? FLASH_SECTOR_1 : FLASH_SECTOR_2;
    erase.NbSectors = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    uint32_t sector_error;
    HAL_IWDG_Refresh(&hiwdg);
    bool ok = HAL_FLASHEx_Erase(&erase, &sector_error) == HAL_OK;
    HAL_IWDG_Refresh(&hiwdg);
    return ok;
}

static bool program_word(void *context, uint32_t bank, uint32_t offset, uint32_t word)
{
    (void)context;
    if (bank > 1 || offset > SETTINGS_BANK_BYTES - 4 || (offset & 3U)) return false;
    uint32_t address = bank_address(bank) + offset;
    return HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, word) == HAL_OK &&
           *(const volatile uint32_t *)address == word;
}

static const SettingsStorage storage = {NULL, read_bank, erase_bank, program_word};

bool SettingsFlash_Load(AppSettings *settings)
{
    return SettingsJournal_Load(&storage, settings);
}

bool SettingsFlash_Save(const AppSettings *settings)
{
    /* Only DataSaveTask calls this function after startup. */
    if (HAL_FLASH_Unlock() != HAL_OK) return false;
    /* Scanning reads erased slots before writing them. Bypass the Flash data
     * cache during the transaction so readback cannot see an old cached word. */
    bool data_cache_enabled = (FLASH->ACR & FLASH_ACR_DCEN) != 0U;
    if (data_cache_enabled) {
        __HAL_FLASH_DATA_CACHE_DISABLE();
        __DSB();
        __ISB();
    }
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                          FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    bool ok = SettingsJournal_Save(&storage, settings);
    if (data_cache_enabled) {
        __HAL_FLASH_DATA_CACHE_RESET();
        __HAL_FLASH_DATA_CACHE_ENABLE();
        __DSB();
        __ISB();
    }
    HAL_FLASH_Lock();
    return ok;
}
