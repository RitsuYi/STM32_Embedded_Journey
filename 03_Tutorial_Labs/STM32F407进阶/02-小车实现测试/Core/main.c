#include "stm32f4xx.h"
#include "ADC_Bat.h"
#include "AppTask.h"
#include "Encoder.h"
#include "Key.h"
#include "MPU6050.h"
#include "Motor.h"
#include "OLED.h"
#include "Timer.h"

int main(void)
{
	Timer_Init();
	OLED_Init();
	Encoder_Init();
	ADC_Bat_Init();
	MPU6050_Init();
	Motor_Init();
	Key_Init();
	AppTask_Init();

	Motor_SetCarState(CAR_STOP);
	OLED_Task();

	while (1)
	{
		AppTask_Run();
		Timer_RunPendingTasks();
	}
}
