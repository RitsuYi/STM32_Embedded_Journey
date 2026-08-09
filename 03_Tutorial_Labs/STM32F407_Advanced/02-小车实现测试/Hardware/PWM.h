#ifndef __PWM_H
#define __PWM_H

#include "stm32f4xx.h"

typedef enum
{
	PWM_CHANNEL_A = 0,
	PWM_CHANNEL_B,
	PWM_CHANNEL_C,
	PWM_CHANNEL_D,
	PWM_CHANNEL_COUNT
} PWM_Channel_t;

void PWM_Init(void);
void PWM_SetDuty(PWM_Channel_t channel, uint16_t duty);
void PWM_StopAll(void);
uint16_t PWM_GetMaxDuty(void);

#endif
