#ifndef __ENCODER_H
#define __ENCODER_H

#include "stm32f10x.h"

void Encoder_Init(void);
int16_t Encoder_Get(void);
void Encoder_Reset(void);
void Encoder_HandleIRQ(void);

#endif
