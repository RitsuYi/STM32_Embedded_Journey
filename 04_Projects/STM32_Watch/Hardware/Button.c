#include "stm32f10x.h"
#include "Button.h"
#include "Delay.h"

/**
  * 函    数：按键初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：初始化PB1为上拉输入模式（编码器按键）
  */
void Button_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);  // 开启GPIOB的时钟

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;          // 上拉输入
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;               // PB1引脚（编码器按键）
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);                  // 初始化PB1
}

/**
  * 函    数：获取按键状态（实时）
  * 参    数：无
  * 返 回 值：1表示按下，0表示未按下
  * 说    明：不带消抖，返回当前电平状态
  */
uint8_t Button_Get(void)
{
	if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == 0)
	{
		return 1;  // 低电平表示按下
	}
	else
	{
		return 0;
	}
}

/**
  * 函    数：获取按键按下事件（带消抖和等待松开）
  * 参    数：无
  * 返 回 值：1表示检测到一次按下事件，0表示未按下
  * 说    明：包含软件消抖和等待松开逻辑，确保每次按下只返回一次
  */
uint8_t Button_Pressed(void)
{
	static uint8_t key_state = 0;  // 按键状态：0-未按下，1-按下消抖中

	if (key_state == 0)
	{
		// 检测到下降沿（按下）
		if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == 0)
		{
			key_state = 1;
			Delay_ms(20);  // 消抖延时
		}
	}
	else
	{
		// 确认是否真的按下
		if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == 0)
		{
			// 确认按下，等待松开
			while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == 0);  // 等待松开
			key_state = 0;
			return 1;  // 返回按下事件
		}
		else
		{
			key_state = 0;  // 误触发，复位状态
		}
	}
	return 0;
}
