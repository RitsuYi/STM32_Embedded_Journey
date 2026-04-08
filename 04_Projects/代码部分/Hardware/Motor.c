#include "stm32f10x.h"                  // Device header
#include "PWM.h"

void Motor_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_10 | GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_ResetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_10 | GPIO_Pin_11);
}

void Motor_SetPWM_L(int16_t Duty)
{
	if (Duty >= 0)
	{
		GPIO_SetBits(GPIOB, GPIO_Pin_10);
		GPIO_ResetBits(GPIOB, GPIO_Pin_11);
		PWM_SetCompare2(Duty);
	}
	else
	{
		GPIO_SetBits(GPIOB, GPIO_Pin_11);
		GPIO_ResetBits(GPIOB, GPIO_Pin_10);
		PWM_SetCompare2(-Duty);
	}

}

void Motor_SetPWM_R(int16_t Duty)
{
	if (Duty >= 0)
		{
			GPIO_SetBits(GPIOB, GPIO_Pin_0); 
			GPIO_ResetBits(GPIOB, GPIO_Pin_1);   
			PWM_SetCompare1(Duty);
		}
		else
		{		
		  GPIO_SetBits(GPIOB, GPIO_Pin_1);
			GPIO_ResetBits(GPIOB, GPIO_Pin_0);
			PWM_SetCompare1(-Duty);
		}	

}




