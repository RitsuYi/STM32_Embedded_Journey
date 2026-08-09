#ifndef __LED_H
#define __LED_H

#include "stm32f4xx.h"

void LED_Init(void);
void LED1_ON(void);
void LED1_OFF(void);
void LED2_ON(void);
void LED2_OFF(void);
void LED1_SetMode(uint8_t Mode);
void LED2_SetMode(uint8_t Mode);
void LED_Tick(void);

#endif
