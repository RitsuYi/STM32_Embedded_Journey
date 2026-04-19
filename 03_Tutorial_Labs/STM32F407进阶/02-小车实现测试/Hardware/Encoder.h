#ifndef __ENCODER_H
#define __ENCODER_H

#include "stm32f4xx.h"

typedef enum
{
	ENCODER_A = 0,
	ENCODER_B,
	ENCODER_C,
	ENCODER_D,
	ENCODER_COUNT
} Encoder_Id_t;

void Encoder_Init(void);
void Encoder_UpdateSpeed(void);
int32_t Encoder_GetCount(Encoder_Id_t encoder);
int32_t Encoder_GetDeltaCount(Encoder_Id_t encoder);
int32_t Encoder_GetSpeedRpm(Encoder_Id_t encoder);
int32_t Encoder_GetLeftSpeedRpm(void);
int32_t Encoder_GetRightSpeedRpm(void);
void Encoder_Clear(Encoder_Id_t encoder);
void Encoder_ClearAll(void);

#endif
