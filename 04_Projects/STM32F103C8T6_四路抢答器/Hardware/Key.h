#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

#define KEY_EVENT_START    0x01
#define KEY_EVENT_RESET    0x02
#define KEY_EVENT_TEAM1    0x04
#define KEY_EVENT_TEAM2    0x08
#define KEY_EVENT_TEAM3    0x10
#define KEY_EVENT_TEAM4    0x20

void Key_Init(void);
void Key_Update(void);
uint8_t Key_GetEvents(void);
uint8_t Key_IsPressed(uint8_t keyIndex);

#endif
