#ifndef __BUTTON_H__
#define __BUTTON_H__

#include "stm32f10x.h"

void Button_Init(void);
uint8_t Button_Get(void);  // 返回1表示按下，0表示未按下
uint8_t Button_Pressed(void);  // 返回1表示单次按下（带消抖和等待松开）

#endif
