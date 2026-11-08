#ifndef SYSTEM_STARTUP_H
#define SYSTEM_STARTUP_H

#include "cmsis_os2.h"
#include "main.h"

#define SYSTEM_STARTUP_READY_FLAG (1U << 0)

extern osEventFlagsId_t SystemStartupEventHandle;

static inline void SystemStartup_WaitReady(void)
{
    uint32_t flags = osEventFlagsWait(SystemStartupEventHandle,
                                      SYSTEM_STARTUP_READY_FLAG,
                                      osFlagsWaitAny | osFlagsNoClear,
                                      osWaitForever);
    if ((flags & osFlagsError) != 0U || (flags & SYSTEM_STARTUP_READY_FLAG) == 0U) {
        Error_Handler();
    }
}

#endif
