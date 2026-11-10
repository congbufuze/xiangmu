#include "HardwareInitTask.h"
// includes
// sys


#include "stm32f4xx_it.h"
#include "delay.h"
#include "tim.h"
#include "rtc.h"
#include "usart.h"
#include "iwdg.h"
#include "SystemStartup.h"
#include "AppServices.h"
#include "AppPower.h"

// user
#include "HWDataAccess.h"
#include "version.h"

// bsp
#include "key.h"
#include "lcd.h"
#include "lcd_init.h"
#include "CST816.h"


#include <stdio.h>


void StartHardwareInitTask(void *argument)
{
  /* USER CODE BEGIN StartHardwareInitTask */
  /* Infinite loop */
    
    HAL_IWDG_Refresh(&hiwdg);
    MX_RTC_Init();
    if(HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 2000, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
    {
      Error_Handler();
    }
    MX_USART1_UART_Init();
    HAL_IWDG_Refresh(&hiwdg);

    // sys delay
    delay_init();
    // wait
    // delay_ms(1000);

    // power
    HWInterface.Power.Init();
    HAL_IWDG_Refresh(&hiwdg);

    // key
    Key_Port_Init();

    // sensors
    uint8_t num = 3;
    while(num && HWInterface.AHT21.ConnectionError)
    {
      num--;
      HWInterface.AHT21.ConnectionError = HWInterface.AHT21.Init();
      HAL_IWDG_Refresh(&hiwdg);
    }

    num = 3;
    while(num && HWInterface.IMU.ConnectionError)
    {
      num--;
      HWInterface.IMU.ConnectionError = HWInterface.IMU.Init();
      HAL_IWDG_Refresh(&hiwdg);
    }

    g_app_diagnostics.aht_init_status = HWInterface.AHT21.ConnectionError;
    g_app_diagnostics.imu_init_status = HWInterface.IMU.ConnectionError;

    num = 3;
    while(num && HWInterface.Ecompass.ConnectionError)
    {
      num--;
      HWInterface.Ecompass.ConnectionError = HWInterface.Ecompass.Init();
    }
    if(!HWInterface.Ecompass.ConnectionError)
      HWInterface.Ecompass.Sleep();

    num = 3;
    while(num && HWInterface.Barometer.ConnectionError)
    {
      num--;
      HWInterface.Barometer.ConnectionError = HWInterface.Barometer.Init();
    }

    num = 3;
    while(num && HWInterface.HR_meter.ConnectionError)
    {
      num--;
      HWInterface.HR_meter.ConnectionError = HWInterface.HR_meter.Init();
    }
    if(!HWInterface.HR_meter.ConnectionError)
      HWInterface.HR_meter.Sleep();
    // BLE
    HWInterface.BLE.Init();
    HWInterface.BLE.Disable();

    // touch
    CST816_GPIO_Init();
    CST816_RESET();
    CST816_Config_AutoSleepTime(5);
    CST816_IIC_WriteREG(IrqCtl, EnTouch | EnChange);

    // lcd
    MX_TIM2_Init();
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    LCD_Init();
    LCD_Fill(0,0, LCD_W, LCD_H, BLACK);
    delay_ms(10);
    AppSettings settings;
    AppSettings_Read(&settings);
    LCD_Set_Light(settings.brightness);
    HAL_IWDG_Refresh(&hiwdg);
    LCD_ShowString(72,LCD_H/2,(uint8_t*)"Welcome!", WHITE, BLACK, 24, 0);
    uint8_t lcd_buf_str[25];
    sprintf((char*)lcd_buf_str, "OV-Watch V%d.%d.%d", watch_version_major(), watch_version_minor(), watch_version_patch());
    LCD_ShowString(34, LCD_H/2+48, (uint8_t*)lcd_buf_str, WHITE, BLACK, 24, 0);
    delay_ms(1000);
    LCD_Fill(0, LCD_H/2-24, LCD_W, LCD_H/2+49, BLACK);


    /* UITask owns LCD/LVGL after READY. */
    AppPower_StartRuntimeClock();

    if ((osEventFlagsSet(SystemStartupEventHandle, SYSTEM_STARTUP_READY_FLAG) & osFlagsError) != 0U) {
      Error_Handler();
    }
    g_app_diagnostics.hardware_stack_free = osThreadGetStackSpace(osThreadGetId());
    osThreadExit();
    
  /* USER CODE END StartHardwareInitTask */
}
