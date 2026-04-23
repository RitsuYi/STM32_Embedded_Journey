#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f4xx.h"

typedef enum
{
	MOTOR_A = 0,
	MOTOR_B,
	MOTOR_C,
	MOTOR_D,
	MOTOR_COUNT
} Motor_Id_t;

typedef enum
{
	CAR_FORWARD = 0,
	CAR_BACKWARD,
	CAR_LEFT,
	CAR_RIGHT,
	CAR_STOP
} Motor_CarState_t;

void Motor_Init(void);
void Motor_SetStandby(FunctionalState newState);
void Motor_Forward(Motor_Id_t motor, uint16_t speed);
void Motor_Backward(Motor_Id_t motor, uint16_t speed);
void Motor_Stop(Motor_Id_t motor);
void Motor_StopAll(void);
void Motor_SetSpeed(Motor_Id_t motor, int16_t speed);

void Motor_CarForward(uint16_t speed);
void Motor_CarBackward(uint16_t speed);
void Motor_CarTurnLeft(uint16_t speed);
void Motor_CarTurnRight(uint16_t speed);
void Motor_CarDriveDifferential(int32_t baseSpeedRpm, int32_t steerCorrectionRpm);
void Motor_CarStop(void);
void Motor_SetSteerCorrectionRpm(int32_t correctionRpm);
void Motor_SetSteerCorrectionMaxRpm(int32_t maxCorrectionRpm);

void Motor_SetCarState(Motor_CarState_t state);
Motor_CarState_t Motor_GetCarState(void);
Motor_CarState_t Motor_GetNextCarState(Motor_CarState_t state);
void Motor_SwitchToNextState(void);
const char *Motor_GetCarStateName(Motor_CarState_t state);
void Motor_ControlTask(void);

void Motor_RefreshPidProfiles(void);
void Motor_SetMoveTargetSpeedRpm(int32_t speedRpm);
void Motor_SetTurnTargetSpeedRpm(int32_t speedRpm);
void Motor_SetHeadingAssistEnabled(uint8_t enable);
int32_t Motor_GetMoveTargetSpeedRpm(void);
int32_t Motor_GetTurnTargetSpeedRpm(void);
int32_t Motor_GetSteerCorrectionRpm(void);

int32_t Motor_GetTargetSpeedRpm(Motor_Id_t motor);
int32_t Motor_GetActualSpeedRpm(Motor_Id_t motor);
int16_t Motor_GetOutputCommand(Motor_Id_t motor);
int16_t Motor_GetOutputPercent(Motor_Id_t motor);
float Motor_GetHeadingCorrectionRpm(void);
uint8_t Motor_IsHeadingCorrectionEnabled(void);
uint8_t Motor_IsHeadingAssistEnabled(void);
uint8_t Motor_IsClosedLoopEnabled(void);

#endif
