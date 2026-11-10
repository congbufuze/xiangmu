#include "key.h"
#include "cmsis_os2.h"
#include "delay.h"
#include "user_TasksInit.h"

void Key_Port_Init(void)
{
  /* PA0 GPIO/EXTI configuration belongs to CubeMX's MX_GPIO_Init(). */
}

uint8_t KeyScan(uint8_t mode)
{
	static uint8_t key_up = 1;
	uint8_t keyvalue=0;
	if(mode) key_up = 1;
	if( key_up && (!KEY1))
	{
		//delay_ms(3);//ensure the key is down
		osDelay(3);
		if(!KEY1) keyvalue = 1;
		if(keyvalue) key_up = 0;
	}
	else
	{
		//delay_ms(3);//ensure the key is up
		osDelay(3);
		if(KEY1)
			key_up = 1;
	}
	return keyvalue;
}

