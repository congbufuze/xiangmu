#ifndef __HARDWARE_INIT_H
#define __HARDWARE_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "cmsis_os.h"

void StartHardwareInitTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __HARDWARE_INIT_H */