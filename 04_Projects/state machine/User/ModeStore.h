#ifndef __MODE_STORE_H
#define __MODE_STORE_H

#include "stm32f10x.h"

#define MODE_STORE_DEFAULT_MODE		1
#define MODE_STORE_MIN_MODE			1
#define MODE_STORE_MAX_MODE			4

uint8_t ModeStore_Load(uint8_t DefaultMode);
uint8_t ModeStore_Save(uint8_t Mode);

#endif
