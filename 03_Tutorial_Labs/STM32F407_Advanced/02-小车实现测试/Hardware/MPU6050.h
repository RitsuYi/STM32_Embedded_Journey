#ifndef __MPU6050_H
#define __MPU6050_H

#include "stm32f4xx.h"

void MPU6050_Init(void);
void MPU6050_SampleTask(void);
void MPU6050_ResetYaw(void);
void MPU6050_StartGyroBiasCalibration(void);

uint8_t MPU6050_IsOnline(void);
uint8_t MPU6050_IsCalibrated(void);
int16_t MPU6050_GetRawGyroX(void);
int16_t MPU6050_GetRawGyroY(void);
int16_t MPU6050_GetRawGyroZ(void);
int16_t MPU6050_GetRawAccelX(void);
int16_t MPU6050_GetRawAccelY(void);
int16_t MPU6050_GetRawAccelZ(void);
float MPU6050_GetGyroXDps(void);
float MPU6050_GetGyroYDps(void);
float MPU6050_GetGyroZDps(void);
float MPU6050_GetAccelXG(void);
float MPU6050_GetAccelYG(void);
float MPU6050_GetAccelZG(void);
float MPU6050_GetPitchAccDeg(void);
float MPU6050_GetRollAccDeg(void);
float MPU6050_GetPitchTiltDeg(void);
float MPU6050_GetRollTiltDeg(void);
float MPU6050_GetAccelNormG(void);
uint8_t MPU6050_IsAccelTiltValid(void);
float MPU6050_GetYawDeg(void);
float MPU6050_GetYawGyroOnlyDeg(void);
float MPU6050_GetGyroBiasZDps(void);
void MPU6050_SetYawGyroOnlyDeg(float yawDeg);
void MPU6050_SetFusedYawDeg(float yawDeg);

#endif
