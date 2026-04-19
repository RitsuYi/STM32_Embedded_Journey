#ifndef __PID_H
#define __PID_H

#include "stm32f4xx.h"

typedef struct
{
	float Target;
	float Actual;
	float Actual1;
	float Out;

	float Kp;
	float Ki;
	float Kd;

	float Error0;
	float Error1;

	float POut;
	float IOut;
	float DOut;

	float IOutMax;
	float IOutMin;

	float OutMax;
	float OutMin;

	float OutOffset;
} PID_t;

typedef struct
{
	float kp;
	float ki;
	float kd;
	float integralLimit;
	float outputLimit;
	float outputOffset;
} PID_SpeedConfig_t;

void PID_LoadSpeedConfig(void);
void PID_GetSpeedConfig(PID_SpeedConfig_t *config);
void PID_SetSpeedTunings(float kp, float ki, float kd);
void PID_SetSpeedLimits(float integralLimit, float outputLimit);

void PID_Init(PID_t *p);
void PID_Reset(PID_t *p);
void PID_ApplySpeedConfig(PID_t *p);
void PID_Update(PID_t *p);
float PID_Calculate(PID_t *p, float target, float actual);

#endif
