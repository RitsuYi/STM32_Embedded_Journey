#ifndef __IMU_FUSION_H
#define __IMU_FUSION_H

#include "stm32f4xx.h"

void IMU_Fusion_Init(void);
void IMU_Fusion_SampleTask(void);

float IMU_Fusion_GetYawDeg(void);
float IMU_Fusion_GetMagHeadingDeg(void);
float IMU_Fusion_GetMagHeadingRawDeg(void);
float IMU_Fusion_GetMagHeadingTiltCompDeg(void);
float IMU_Fusion_GetGyroHeadingDeg(void);
uint8_t IMU_Fusion_IsMagCorrectionValid(void);
uint8_t IMU_Fusion_IsMagRecoveryPending(void);

void IMU_Fusion_ResetYaw(float yawDeg);

#endif
