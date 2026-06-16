#ifndef __LED_H
#define __LED_H

#include "stm32f10x.h"

void LED_Init(void);
void LED_AllOff(void);
void LED_RunOn(void);
void LED_RunOff(void);
void LED_RunToggle(void);
void LED_TeamAllOff(void);
void LED_TeamOn(uint8_t team);
void LED_SetWinner(uint8_t team);
void LED_AlarmOn(void);
void LED_AlarmOff(void);
void LED_AlarmToggle(void);

#endif
