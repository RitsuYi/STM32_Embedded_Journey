#ifndef __HMC5883L_H
#define __HMC5883L_H

#include "stm32f4xx.h"

void HMC5883L_Init(void);
void HMC5883L_SampleTask(void);

uint8_t HMC5883L_IsOnline(void);
uint8_t HMC5883L_IsCalibrated(void);
uint8_t HMC5883L_IsCalibrating(void);
uint8_t HMC5883L_IsHeadingValid(void);

int16_t HMC5883L_GetRawX(void);
int16_t HMC5883L_GetRawY(void);
int16_t HMC5883L_GetRawZ(void);

float HMC5883L_GetFieldX(void);
float HMC5883L_GetFieldY(void);
float HMC5883L_GetFieldZ(void);

float HMC5883L_GetHeadingDeg(void);

void HMC5883L_StartCalibration(void);
void HMC5883L_StopCalibration(void);
void HMC5883L_ResetCalibration(void);

#endif
