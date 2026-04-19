#ifndef __MPU6050_H
#define __MPU6050_H

#include "stm32f4xx.h"

void MPU6050_Init(void);
void MPU6050_SampleTask(void);
void MPU6050_ResetYaw(void);
void MPU6050_StartGyroBiasCalibration(void);

uint8_t MPU6050_IsOnline(void);
uint8_t MPU6050_IsCalibrated(void);
int16_t MPU6050_GetRawGyroZ(void);
float MPU6050_GetGyroZDps(void);
float MPU6050_GetYawDeg(void);
float MPU6050_GetGyroBiasZDps(void);

#endif
