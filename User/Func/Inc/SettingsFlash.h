#ifndef SETTINGS_FLASH_H
#define SETTINGS_FLASH_H
#include "AppTypes.h"
bool SettingsFlash_Load(AppSettings *settings);
bool SettingsFlash_Save(const AppSettings *settings);
#endif
