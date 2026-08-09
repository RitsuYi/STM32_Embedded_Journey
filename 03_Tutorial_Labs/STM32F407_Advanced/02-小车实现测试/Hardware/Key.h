#ifndef __KEY_H
#define __KEY_H

#include "stm32f4xx.h"

void Key_Init(void);
uint8_t Key_Scan(void);
uint8_t Key_IsPressed(void);

#endif
