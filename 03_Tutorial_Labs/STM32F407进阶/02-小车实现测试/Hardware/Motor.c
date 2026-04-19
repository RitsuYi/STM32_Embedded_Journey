#include "Motor.h"
#include "BoardConfig.h"
#include "Encoder.h"
#include "PID.h"
#include "PWM.h"

typedef struct
{
	GPIO_TypeDef *in1Port;
	uint16_t in1Pin;
	GPIO_TypeDef *in2Port;
	uint16_t in2Pin;
	PWM_Channel_t pwmChannel;
	uint8_t reverseFlag;
} Motor_Config_t;

static const Motor_Config_t g_motorConfig[MOTOR_COUNT] =
{
	{BOARD_MOTOR_A_IN1_PORT, BOARD_MOTOR_A_IN1_PIN, BOARD_MOTOR_A_IN2_PORT, BOARD_MOTOR_A_IN2_PIN, PWM_CHANNEL_A, BOARD_MOTOR_A_REVERSE},
	{BOARD_MOTOR_B_IN1_PORT, BOARD_MOTOR_B_IN1_PIN, BOARD_MOTOR_B_IN2_PORT, BOARD_MOTOR_B_IN2_PIN, PWM_CHANNEL_B, BOARD_MOTOR_B_REVERSE},
	{BOARD_MOTOR_C_IN1_PORT, BOARD_MOTOR_C_IN1_PIN, BOARD_MOTOR_C_IN2_PORT, BOARD_MOTOR_C_IN2_PIN, PWM_CHANNEL_C, BOARD_MOTOR_C_REVERSE},
	{BOARD_MOTOR_D_IN1_PORT, BOARD_MOTOR_D_IN1_PIN, BOARD_MOTOR_D_IN2_PORT, BOARD_MOTOR_D_IN2_PIN, PWM_CHANNEL_D, BOARD_MOTOR_D_REVERSE}
};

static PID_t g_speedPid[MOTOR_COUNT];
static int32_t g_targetSpeedRpm[MOTOR_COUNT];
static int16_t g_outputCommand[MOTOR_COUNT];
static int32_t g_moveTargetSpeedRpm = BOARD_CAR_MOVE_TARGET_SPEED_RPM;
static int32_t g_turnTargetSpeedRpm = BOARD_CAR_TURN_TARGET_SPEED_RPM;
static uint8_t g_closedLoopEnabled = 0U;
static Motor_CarState_t g_carState = CAR_STOP;

typedef enum
{
	CAR_WHEEL_LEFT_FRONT = 0,
	CAR_WHEEL_RIGHT_FRONT,
	CAR_WHEEL_LEFT_REAR,
	CAR_WHEEL_RIGHT_REAR,
	CAR_WHEEL_COUNT
} Car_WheelPosition_t;

/* Keep wheel order and motor/encoder pairing fully driven by BoardConfig. */
static const Motor_Id_t g_carWheelMotorMap[CAR_WHEEL_COUNT] =
{
	(Motor_Id_t)BOARD_CAR_LEFT_FRONT_MOTOR,
	(Motor_Id_t)BOARD_CAR_RIGHT_FRONT_MOTOR,
	(Motor_Id_t)BOARD_CAR_LEFT_REAR_MOTOR,
	(Motor_Id_t)BOARD_CAR_RIGHT_REAR_MOTOR
};

static const Encoder_Id_t g_motorEncoderMap[MOTOR_COUNT] =
{
	[BOARD_CAR_LEFT_FRONT_MOTOR] = (Encoder_Id_t)BOARD_CAR_LEFT_FRONT_ENCODER,
	[BOARD_CAR_RIGHT_FRONT_MOTOR] = (Encoder_Id_t)BOARD_CAR_RIGHT_FRONT_ENCODER,
	[BOARD_CAR_LEFT_REAR_MOTOR] = (Encoder_Id_t)BOARD_CAR_LEFT_REAR_ENCODER,
	[BOARD_CAR_RIGHT_REAR_MOTOR] = (Encoder_Id_t)BOARD_CAR_RIGHT_REAR_ENCODER
};

static void Motor_GPIOClockCmd(GPIO_TypeDef *gpiox, FunctionalState newState)
{
	if (gpiox == GPIOA)
	{
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, newState);
	}
	else if (gpiox == GPIOB)
	{
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, newState);
	}
	else if (gpiox == GPIOC)
	{
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, newState);
	}
	else if (gpiox == GPIOD)
	{
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, newState);
	}
	else if (gpiox == GPIOE)
	{
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, newState);
	}
}

static int16_t Motor_LimitCommand(int16_t speed)
{
	int16_t maxDuty;

	maxDuty = (int16_t)PWM_GetMaxDuty();
	if (speed > maxDuty)
	{
		return maxDuty;
	}

	if (speed < -maxDuty)
	{
		return (int16_t)(-maxDuty);
	}

	return speed;
}

static void Motor_InitPin(GPIO_TypeDef *port, uint16_t pin)
{
	GPIO_InitTypeDef gpioInitStructure;

	Motor_GPIOClockCmd(port, ENABLE);

	gpioInitStructure.GPIO_Pin = pin;
	gpioInitStructure.GPIO_Mode = GPIO_Mode_OUT;
	gpioInitStructure.GPIO_OType = GPIO_OType_PP;
	gpioInitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	gpioInitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(port, &gpioInitStructure);
}

static void Motor_SetDirection(Motor_Id_t motor, BitAction in1Level, BitAction in2Level)
{
	if (motor >= MOTOR_COUNT)
	{
		return;
	}

	GPIO_WriteBit(g_motorConfig[motor].in1Port, g_motorConfig[motor].in1Pin, in1Level);
	GPIO_WriteBit(g_motorConfig[motor].in2Port, g_motorConfig[motor].in2Pin, in2Level);
}

static int16_t Motor_ApplyReverseFlag(Motor_Id_t motor, int16_t speed)
{
	if (motor >= MOTOR_COUNT)
	{
		return 0;
	}

	if (g_motorConfig[motor].reverseFlag != 0U)
	{
		speed = (int16_t)(-speed);
	}

	return speed;
}

static void Motor_ApplyDriveCommand(Motor_Id_t motor, int16_t speed)
{
	int16_t actualSpeed;
	uint16_t duty;

	if (motor >= MOTOR_COUNT)
	{
		return;
	}

	speed = Motor_LimitCommand(speed);
	g_outputCommand[motor] = speed;
	actualSpeed = Motor_ApplyReverseFlag(motor, speed);

	if (actualSpeed > 0)
	{
		Motor_SetStandby(ENABLE);
		Motor_SetDirection(motor, Bit_SET, Bit_RESET);
		PWM_SetDuty(g_motorConfig[motor].pwmChannel, (uint16_t)actualSpeed);
	}
	else if (actualSpeed < 0)
	{
		Motor_SetStandby(ENABLE);
		Motor_SetDirection(motor, Bit_RESET, Bit_SET);
		duty = (uint16_t)(-actualSpeed);
		PWM_SetDuty(g_motorConfig[motor].pwmChannel, duty);
	}
	else
	{
		PWM_SetDuty(g_motorConfig[motor].pwmChannel, 0U);
		Motor_SetDirection(motor, Bit_RESET, Bit_RESET);
	}
}

static void Motor_ResetControlLoops(void)
{
	uint8_t index;

	for (index = 0U; index < MOTOR_COUNT; index++)
	{
		PID_Reset(&g_speedPid[index]);
		g_targetSpeedRpm[index] = 0;
		g_outputCommand[index] = 0;
	}
}

static void Motor_SyncPidProfiles(void)
{
	uint8_t index;

	for (index = 0U; index < MOTOR_COUNT; index++)
	{
		PID_ApplySpeedConfig(&g_speedPid[index]);
	}
}

static void Motor_DisableClosedLoop(void)
{
	g_closedLoopEnabled = 0U;
	Motor_ResetControlLoops();
}

static void Motor_SetLeftRightTargets(int32_t leftTarget, int32_t rightTarget)
{
	g_targetSpeedRpm[g_carWheelMotorMap[CAR_WHEEL_LEFT_FRONT]] = leftTarget;
	g_targetSpeedRpm[g_carWheelMotorMap[CAR_WHEEL_LEFT_REAR]] = leftTarget;
	g_targetSpeedRpm[g_carWheelMotorMap[CAR_WHEEL_RIGHT_FRONT]] = rightTarget;
	g_targetSpeedRpm[g_carWheelMotorMap[CAR_WHEEL_RIGHT_REAR]] = rightTarget;
}

static void Motor_UpdateTargetsByState(void)
{
	int32_t baseTarget;

	switch (g_carState)
	{
		case CAR_FORWARD:
			baseTarget = g_moveTargetSpeedRpm;
			Motor_SetLeftRightTargets(baseTarget, baseTarget);
			break;

		case CAR_BACKWARD:
			baseTarget = -g_moveTargetSpeedRpm;
			Motor_SetLeftRightTargets(baseTarget, baseTarget);
			break;

		case CAR_LEFT:
			Motor_SetLeftRightTargets(-g_turnTargetSpeedRpm, g_turnTargetSpeedRpm);
			return;

		case CAR_RIGHT:
			Motor_SetLeftRightTargets(g_turnTargetSpeedRpm, -g_turnTargetSpeedRpm);
			return;

		case CAR_STOP:
		default:
			Motor_SetLeftRightTargets(0, 0);
			return;
	}
}

static void Motor_EnableClosedLoop(Motor_CarState_t state)
{
	g_carState = state;
	g_closedLoopEnabled = 1U;
	Motor_ResetControlLoops();
	Motor_SyncPidProfiles();
	Motor_ControlTask();
}

void Motor_Init(void)
{
	uint8_t index;

	PWM_Init();

	Motor_InitPin(BOARD_MOTOR_A_IN1_PORT, BOARD_MOTOR_A_IN1_PIN);
	Motor_InitPin(BOARD_MOTOR_A_IN2_PORT, BOARD_MOTOR_A_IN2_PIN);
	Motor_InitPin(BOARD_MOTOR_B_IN1_PORT, BOARD_MOTOR_B_IN1_PIN);
	Motor_InitPin(BOARD_MOTOR_B_IN2_PORT, BOARD_MOTOR_B_IN2_PIN);
	Motor_InitPin(BOARD_MOTOR_C_IN1_PORT, BOARD_MOTOR_C_IN1_PIN);
	Motor_InitPin(BOARD_MOTOR_C_IN2_PORT, BOARD_MOTOR_C_IN2_PIN);
	Motor_InitPin(BOARD_MOTOR_D_IN1_PORT, BOARD_MOTOR_D_IN1_PIN);
	Motor_InitPin(BOARD_MOTOR_D_IN2_PORT, BOARD_MOTOR_D_IN2_PIN);

	RCC_AHB1PeriphClockCmd(BOARD_MOTOR_STBY_GPIO_RCC, ENABLE);
	Motor_InitPin(BOARD_MOTOR_STBY_GPIO_PORT, BOARD_MOTOR_STBY_PIN);

	PID_LoadSpeedConfig();

	for (index = 0U; index < MOTOR_COUNT; index++)
	{
		PID_Init(&g_speedPid[index]);
		PID_ApplySpeedConfig(&g_speedPid[index]);
		g_targetSpeedRpm[index] = 0;
		g_outputCommand[index] = 0;
	}

	Motor_CarStop();
}

void Motor_SetStandby(FunctionalState newState)
{
	if (newState != DISABLE)
	{
		GPIO_SetBits(BOARD_MOTOR_STBY_GPIO_PORT, BOARD_MOTOR_STBY_PIN);
	}
	else
	{
		GPIO_ResetBits(BOARD_MOTOR_STBY_GPIO_PORT, BOARD_MOTOR_STBY_PIN);
	}
}

void Motor_Forward(Motor_Id_t motor, uint16_t speed)
{
	Motor_SetSpeed(motor, (int16_t)speed);
}

void Motor_Backward(Motor_Id_t motor, uint16_t speed)
{
	Motor_SetSpeed(motor, (int16_t)(-((int16_t)speed)));
}

void Motor_Stop(Motor_Id_t motor)
{
	if (motor >= MOTOR_COUNT)
	{
		return;
	}

	g_targetSpeedRpm[motor] = 0;
	Motor_ApplyDriveCommand(motor, 0);
}

void Motor_StopAll(void)
{
	uint8_t index;

	for (index = 0U; index < MOTOR_COUNT; index++)
	{
		Motor_Stop((Motor_Id_t)index);
	}

	Motor_SetStandby(DISABLE);
}

void Motor_SetSpeed(Motor_Id_t motor, int16_t speed)
{
	if (motor >= MOTOR_COUNT)
	{
		return;
	}

	Motor_ApplyDriveCommand(motor, speed);
}

void Motor_CarForward(uint16_t speed)
{
	Motor_SetMoveTargetSpeedRpm((int32_t)speed);
	Motor_EnableClosedLoop(CAR_FORWARD);
}

void Motor_CarBackward(uint16_t speed)
{
	Motor_SetMoveTargetSpeedRpm((int32_t)speed);
	Motor_EnableClosedLoop(CAR_BACKWARD);
}

void Motor_CarTurnLeft(uint16_t speed)
{
	Motor_SetTurnTargetSpeedRpm((int32_t)speed);
	Motor_EnableClosedLoop(CAR_LEFT);
}

void Motor_CarTurnRight(uint16_t speed)
{
	Motor_SetTurnTargetSpeedRpm((int32_t)speed);
	Motor_EnableClosedLoop(CAR_RIGHT);
}

void Motor_CarStop(void)
{
	g_carState = CAR_STOP;
	Motor_DisableClosedLoop();
	Motor_StopAll();
}

void Motor_SetCarState(Motor_CarState_t state)
{
	switch (state)
	{
		case CAR_FORWARD:
			Motor_CarForward((uint16_t)g_moveTargetSpeedRpm);
			break;

		case CAR_BACKWARD:
			Motor_CarBackward((uint16_t)g_moveTargetSpeedRpm);
			break;

		case CAR_LEFT:
			Motor_CarTurnLeft((uint16_t)g_turnTargetSpeedRpm);
			break;

		case CAR_RIGHT:
			Motor_CarTurnRight((uint16_t)g_turnTargetSpeedRpm);
			break;

		case CAR_STOP:
		default:
			Motor_CarStop();
			break;
	}
}

Motor_CarState_t Motor_GetCarState(void)
{
	return g_carState;
}

Motor_CarState_t Motor_GetNextCarState(Motor_CarState_t state)
{
	switch (state)
	{
		case CAR_FORWARD:
			return CAR_BACKWARD;

		case CAR_BACKWARD:
			return CAR_LEFT;

		case CAR_LEFT:
			return CAR_RIGHT;

		case CAR_RIGHT:
			return CAR_STOP;

		case CAR_STOP:
		default:
			return CAR_FORWARD;
	}
}

void Motor_SwitchToNextState(void)
{
	Motor_SetCarState(Motor_GetNextCarState(g_carState));
}

const char *Motor_GetCarStateName(Motor_CarState_t state)
{
	switch (state)
	{
		case CAR_FORWARD:
			return "FORWARD";

		case CAR_BACKWARD:
			return "BACKWARD";

		case CAR_LEFT:
			return "LEFT";

		case CAR_RIGHT:
			return "RIGHT";

		case CAR_STOP:
		default:
			return "STOP";
	}
}

void Motor_ControlTask(void)
{
	uint8_t index;
	int32_t actualSpeed;
	int16_t pwmCommand;

	if (g_closedLoopEnabled == 0U)
	{
		return;
	}

	Motor_UpdateTargetsByState();

	for (index = 0U; index < MOTOR_COUNT; index++)
	{
		actualSpeed = Encoder_GetSpeedRpm(g_motorEncoderMap[index]);
		pwmCommand = (int16_t)PID_Calculate(&g_speedPid[index],
											(float)g_targetSpeedRpm[index],
											(float)actualSpeed);
		Motor_ApplyDriveCommand((Motor_Id_t)index, pwmCommand);
	}
}

void Motor_RefreshPidProfiles(void)
{
	Motor_SyncPidProfiles();
}

void Motor_SetMoveTargetSpeedRpm(int32_t speedRpm)
{
	if (speedRpm < 0)
	{
		speedRpm = -speedRpm;
	}

	if (speedRpm > BOARD_CAR_MOVE_TARGET_SPEED_MAX_RPM)
	{
		speedRpm = BOARD_CAR_MOVE_TARGET_SPEED_MAX_RPM;
	}

	g_moveTargetSpeedRpm = speedRpm;
}

void Motor_SetTurnTargetSpeedRpm(int32_t speedRpm)
{
	if (speedRpm < 0)
	{
		speedRpm = -speedRpm;
	}

	if (speedRpm > BOARD_CAR_TURN_TARGET_SPEED_MAX_RPM)
	{
		speedRpm = BOARD_CAR_TURN_TARGET_SPEED_MAX_RPM;
	}

	g_turnTargetSpeedRpm = speedRpm;
}

void Motor_SetHeadingAssistEnabled(uint8_t enable)
{
	(void)enable;
}

int32_t Motor_GetMoveTargetSpeedRpm(void)
{
	return g_moveTargetSpeedRpm;
}

int32_t Motor_GetTurnTargetSpeedRpm(void)
{
	return g_turnTargetSpeedRpm;
}

int32_t Motor_GetTargetSpeedRpm(Motor_Id_t motor)
{
	if (motor >= MOTOR_COUNT)
	{
		return 0;
	}

	return g_targetSpeedRpm[motor];
}

int32_t Motor_GetActualSpeedRpm(Motor_Id_t motor)
{
	if (motor >= MOTOR_COUNT)
	{
		return 0;
	}

	return Encoder_GetSpeedRpm(g_motorEncoderMap[motor]);
}

int16_t Motor_GetOutputCommand(Motor_Id_t motor)
{
	if (motor >= MOTOR_COUNT)
	{
		return 0;
	}

	return g_outputCommand[motor];
}

int16_t Motor_GetOutputPercent(Motor_Id_t motor)
{
	int32_t scaledValue;
	int32_t maxDuty;

	if (motor >= MOTOR_COUNT)
	{
		return 0;
	}

	maxDuty = (int32_t)PWM_GetMaxDuty();
	if (maxDuty <= 0)
	{
		return 0;
	}

	scaledValue = (int32_t)g_outputCommand[motor] * 100;
	if (scaledValue >= 0)
	{
		scaledValue = (scaledValue + (maxDuty / 2)) / maxDuty;
	}
	else
	{
		scaledValue = -(((-scaledValue) + (maxDuty / 2)) / maxDuty);
	}

	return (int16_t)scaledValue;
}

float Motor_GetHeadingCorrectionRpm(void)
{
	return 0.0f;
}

uint8_t Motor_IsHeadingCorrectionEnabled(void)
{
	return 0U;
}

uint8_t Motor_IsHeadingAssistEnabled(void)
{
	return 0U;
}

uint8_t Motor_IsClosedLoopEnabled(void)
{
	return g_closedLoopEnabled;
}
