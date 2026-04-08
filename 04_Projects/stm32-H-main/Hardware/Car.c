#include "stm32f10x.h"                  // Device header
#include "Motor.h"

void Car_Init()
{
	Motor_Init();
}

void Car_forward()
{
	Motor_SetPWM(1,40);
	Motor_SetPWM(2,40);
}

void Car_Right()
{
	Motor_SetPWM(1,30);
	Motor_SetPWM(2,40);
}

void Car_Right1()
{
	Motor_SetPWM(1,35);
	Motor_SetPWM(2,40);
}

void Car_Left()
{
	Motor_SetPWM(1,40);
	Motor_SetPWM(2,30);
}

void Car_Left1()
{
	Motor_SetPWM(1,40);
	Motor_SetPWM(2,35);
}

void Car_Stop()
{
	Motor_SetPWM(1,0);
	Motor_SetPWM(2,0);
}