#include "stm32f10x.h"                  // Device header

int main(void)
{
//	RCC->APB2ENR = 0x00000010;//16进制
//	GPIOC->CRH = 0x00300000;
//	GPIOC->ODR = 0x00002000;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,ENABLE);//开启GPIOC外设的时钟（唤醒GPIOC，用引脚前必须先开时钟）
	GPIO_InitTypeDef GPIO_InitStructure;//定义GPIO配置结构体变量（相当于准备一个“配置模板”，用来填引脚的参数）
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;//设置引脚工作模式：推挽输出（普通控制LED/继电器常用的输出模式）
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;//设置要配置的引脚：GPIOC的13号引脚（简称PC13）
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;//设置引脚输出速度：50MHz（输出模式才需要，速度越快电平切换越快）
	GPIO_Init(GPIOC,&GPIO_InitStructure);//调用初始化函数：把上面填好的“模板”传给函数，让硬件按模板配置PC13
//	GPIO_SetBits(GPIOC,GPIO_Pin_13);//设置13号引脚高电平
	GPIO_ResetBits(GPIOC,GPIO_Pin_13);//设置13号引脚低电平
	while(1)
	{
		
	}
}
