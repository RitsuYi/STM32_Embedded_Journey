#include "stm32f10x.h"                  // Device header
#include "Car.h"

void Track_Init()
{
	Car_Init();
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	
	GPIO_InitTypeDef GPIO_InitStructure;
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_3|GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_4|GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOB,&GPIO_InitStructure);
	
}

uint8_t Read_Left1()
{
	return GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_3);
}
uint8_t Read_Left2()
{
	return GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_2);
}
uint8_t Read_Mid()
{
	return GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_4);
}
uint8_t Read_Right1()
{
	return GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_4);
}
uint8_t Read_Right2()
{
	return GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_5);
}

void Track_Run()
{
	uint8_t L1,L2,M,R1,R2;
	
	L1=Read_Left1();
	L2=Read_Left2();
	M=Read_Mid();
	R1=Read_Right1();
	R2=Read_Right2();
	
	if(L1==0&&L2==0&&M==1&&R1==0&&R2==0)//Ç°½ø
	{
		Car_forward();
	}
	else if(L1==0&&L2==1&&M==0&&R1==0&&R2==0)
	{
		Car_Left();
	}
	else if(L1==1&&L2==0&&M==0&&R1==0&&R2==0)
	{
		Car_Left();
	}
	
	else if(L1==0&&L2==0&&M==0&&R1==1&&R2==0)
	{
		Car_Right();
	}
	else if(L1==0&&L2==0&&M==0&&R1==0&&R2==1)
	{
		Car_Right();
	}
	else if(L1==0&&L2==0&&M==0&&R1==0&&R2==0)
	{
		Car_Stop();
	}
	
	
}