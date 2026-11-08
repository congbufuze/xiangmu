/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "AppServices.h"
#include "SystemStartup.h"
#include "iwdg.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osEventFlagsId_t AppEventHandle, DataSaveEventHandle;
osEventFlagsId_t SystemStartupEventHandle;
/* Event groups are application-owned; CubeMX has no event-group editor here. */
/* USER CODE END Variables */
/* Definitions for the CubeMX-configured save task and shared mutexes */
osMutexId_t SensorMutexHandle, SettingsMutexHandle;
osThreadId_t DataSaveTaskHandle;
static const osThreadAttr_t DataSaveTask_attributes = {
  .name = "DataSaveTask", .stack_size = 384 * 4, .priority = osPriorityLow
};
static const osMutexAttr_t SensorMutex_attributes = {
  .name = "SensorMutex", .attr_bits = osMutexPrioInherit
};
static const osMutexAttr_t SettingsMutex_attributes = {
  .name = "SettingsMutex", .attr_bits = osMutexPrioInherit
};

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for HardwareInitTas */
osThreadId_t HardwareInitTasHandle;
const osThreadAttr_t HardwareInitTas_attributes = {
  .name = "HardwareInitTas",
  .stack_size = 768 * 4,
  .priority = (osPriority_t) osPriorityHigh2,
};
/* Definitions for KeyTask */
osThreadId_t KeyTaskHandle;
const osThreadAttr_t KeyTask_attributes = {
  .name = "KeyTask",
  .stack_size = 192 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for AHT21Task */
osThreadId_t AHT21TaskHandle;
const osThreadAttr_t AHT21Task_attributes = {
  .name = "AHT21Task",
  .stack_size = 384 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for UITask */
osThreadId_t UITaskHandle;
const osThreadAttr_t UITask_attributes = {
  .name = "UITask",
  .stack_size = 1536 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for KEYQueue */
osMessageQueueId_t KEYQueueHandle;
const osMessageQueueAttr_t KEYQueue_attributes = {
  .name = "KEYQueue"
};






/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartDataSaveTask(void *argument);

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartHardwareInitTask(void *argument);
void StartKeyTask(void *argument);
void StartAHT21Task(void *argument);
void StartUITask(void *argument);
void StartDataSaveTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  SensorMutexHandle = osMutexNew(&SensorMutex_attributes);
  SettingsMutexHandle = osMutexNew(&SettingsMutex_attributes);
  /* USER CODE BEGIN RTOS_MUTEX */
  AppEventHandle = osEventFlagsNew(NULL);
  DataSaveEventHandle = osEventFlagsNew(NULL);
  SystemStartupEventHandle = osEventFlagsNew(NULL);
  if (!SystemStartupEventHandle) Error_Handler();
  if (!SensorMutexHandle || !SettingsMutexHandle || !AppEventHandle || !DataSaveEventHandle) Error_Handler();
  AppServices_Init();
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */


  /* Create the queue(s) */
  /* creation of KEYQueue */
  KEYQueueHandle = osMessageQueueNew (8, sizeof(uint8_t), &KEYQueue_attributes);









  /* USER CODE BEGIN RTOS_QUEUES */
  if (KEYQueueHandle == NULL) Error_Handler();
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of HardwareInitTas */
  HardwareInitTasHandle = osThreadNew(StartHardwareInitTask, NULL, &HardwareInitTas_attributes);

  /* creation of KeyTask */
  KeyTaskHandle = osThreadNew(StartKeyTask, NULL, &KeyTask_attributes);

  /* creation of AHT21Task */
  AHT21TaskHandle = osThreadNew(StartAHT21Task, NULL, &AHT21Task_attributes);

  /* creation of UITask */
  UITaskHandle = osThreadNew(StartUITask, NULL, &UITask_attributes);

  DataSaveTaskHandle = osThreadNew(StartDataSaveTask, NULL, &DataSaveTask_attributes);
  /* USER CODE BEGIN RTOS_THREADS */
  if (defaultTaskHandle == NULL || HardwareInitTasHandle == NULL ||
      KeyTaskHandle == NULL || AHT21TaskHandle == NULL || UITaskHandle == NULL) {
    Error_Handler();
  }
  if (!DataSaveTaskHandle) Error_Handler();
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  SystemStartup_WaitReady();
  /* Infinite loop */
  for(;;)
  {
    if (HAL_IWDG_Refresh(&hiwdg) != HAL_OK) {
      Error_Handler();
    }
    AppDiagnostics_Update();
    osDelay(App_MsToTicks(1000));
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartHardwareInitTask */
/**
* @brief Function implementing the HardwareInitTas thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartHardwareInitTask */
__weak void StartHardwareInitTask(void *argument)
{
  /* USER CODE BEGIN StartHardwareInitTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartHardwareInitTask */
}

/* USER CODE BEGIN Header_StartKeyTask */
/**
* @brief Function implementing the KeyTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartKeyTask */
__weak void StartKeyTask(void *argument)
{
  /* USER CODE BEGIN StartKeyTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartKeyTask */
}

/* USER CODE BEGIN Header_StartAHT21Task */
/**
* @brief Function implementing the AHT21Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartAHT21Task */
__weak void StartAHT21Task(void *argument)
{
  /* USER CODE BEGIN StartAHT21Task */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartAHT21Task */
}

/* USER CODE BEGIN Header_StartUITask */
/**
* @brief Function implementing the UITask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartUITask */
__weak void StartUITask(void *argument)
{
  /* USER CODE BEGIN StartUITask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartUITask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Header_StartDataSaveTask */
/* USER CODE END Header_StartDataSaveTask */
__weak void StartDataSaveTask(void *argument)
{
  /* USER CODE BEGIN StartDataSaveTask */
  (void)argument;
  for (;;) osDelay(1);
  /* USER CODE END StartDataSaveTask */
}

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void)
{
  g_app_diagnostics.fault = 1;
  taskDISABLE_INTERRUPTS();
  for (;;) {}
}
/* USER CODE END 5 */
/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
  (void)task;
  g_app_diagnostics.fault = 2;
  g_app_diagnostics.fault_task = name;
  taskDISABLE_INTERRUPTS();
  for (;;) {}
}
/* USER CODE END 4 */

/* USER CODE BEGIN Application */

/* USER CODE END Application */

