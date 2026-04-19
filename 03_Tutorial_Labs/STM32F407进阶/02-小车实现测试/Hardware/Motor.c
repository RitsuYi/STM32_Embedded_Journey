/*
 * 电机控制模块
 *
 * 这个文件位于“执行层”和“整车运动层”的中间，主要职责有：
 * 1. 初始化电机方向控制引脚、待机引脚和 PWM 输出。
 * 2. 提供单个电机的正转、反转、停止、速度设置接口。
 * 3. 维护整车的运动状态（前进、后退、左转、右转、停止）。
 * 4. 将整车目标状态转换成 4 个轮子的目标转速。
 * 5. 在定时任务中读取编码器速度，调用 PID 计算，输出 PWM 驱动命令。
 *
 * 需要注意的是：
 * - g_targetSpeedRpm[] 表示“闭环目标速度”。
 * - g_outputCommand[] 表示“最终输出给 PWM 的控制量”，本质上是占空比命令。
 * - 具体某个电机的正方向是否需要翻转，由 reverseFlag 统一处理。
 */
#include "Motor.h"
#include "BoardConfig.h"
#include "Encoder.h"
#include "PID.h"
#include "PWM.h"

/* 单个电机的硬件资源描述：
 * - IN1/IN2 用于控制 H 桥方向。
 * - pwmChannel 用于输出调速占空比。
 * - reverseFlag 用于修正安装方向与逻辑方向不一致的问题。 */
typedef struct
{
	GPIO_TypeDef *in1Port;
	uint16_t in1Pin;
	GPIO_TypeDef *in2Port;
	uint16_t in2Pin;
	PWM_Channel_t pwmChannel;
	uint8_t reverseFlag;
} Motor_Config_t;

/* 4 个电机的静态硬件配置表。 */
static const Motor_Config_t g_motorConfig[MOTOR_COUNT] =
{
	{BOARD_MOTOR_A_IN1_PORT, BOARD_MOTOR_A_IN1_PIN, BOARD_MOTOR_A_IN2_PORT, BOARD_MOTOR_A_IN2_PIN, PWM_CHANNEL_A, BOARD_MOTOR_A_REVERSE},
	{BOARD_MOTOR_B_IN1_PORT, BOARD_MOTOR_B_IN1_PIN, BOARD_MOTOR_B_IN2_PORT, BOARD_MOTOR_B_IN2_PIN, PWM_CHANNEL_B, BOARD_MOTOR_B_REVERSE},
	{BOARD_MOTOR_C_IN1_PORT, BOARD_MOTOR_C_IN1_PIN, BOARD_MOTOR_C_IN2_PORT, BOARD_MOTOR_C_IN2_PIN, PWM_CHANNEL_C, BOARD_MOTOR_C_REVERSE},
	{BOARD_MOTOR_D_IN1_PORT, BOARD_MOTOR_D_IN1_PIN, BOARD_MOTOR_D_IN2_PORT, BOARD_MOTOR_D_IN2_PIN, PWM_CHANNEL_D, BOARD_MOTOR_D_REVERSE}
};

/* 每个电机各自拥有一套速度 PID 控制器。 */
static PID_t g_speedPid[MOTOR_COUNT];
/* 目标转速数组，单位 RPM。 */
static int32_t g_targetSpeedRpm[MOTOR_COUNT];
/* 最终输出命令数组，带符号。
 * 正负号代表方向，绝对值代表 PWM 占空比大小。 */
static int16_t g_outputCommand[MOTOR_COUNT];
/* 小车直行和转向时的默认目标速度。 */
static int32_t g_moveTargetSpeedRpm = BOARD_CAR_MOVE_TARGET_SPEED_RPM;
static int32_t g_turnTargetSpeedRpm = BOARD_CAR_TURN_TARGET_SPEED_RPM;
/* 闭环控制使能标志：
 * 为 0 时，Motor_ControlTask() 直接返回，不参与 PID 调速。 */
static uint8_t g_closedLoopEnabled = 0U;
/* 当前整车状态。 */
static Motor_CarState_t g_carState = CAR_STOP;

/* 逻辑轮位定义。 */
typedef enum
{
	CAR_WHEEL_LEFT_FRONT = 0,
	CAR_WHEEL_RIGHT_FRONT,
	CAR_WHEEL_LEFT_REAR,
	CAR_WHEEL_RIGHT_REAR,
	CAR_WHEEL_COUNT
} Car_WheelPosition_t;

/* 逻辑轮位到电机编号的映射。
 * 这样整车逻辑只处理“左前/右后”概念，具体落到哪个电机由 BoardConfig 决定。 */
static const Motor_Id_t g_carWheelMotorMap[CAR_WHEEL_COUNT] =
{
	(Motor_Id_t)BOARD_CAR_LEFT_FRONT_MOTOR,
	(Motor_Id_t)BOARD_CAR_RIGHT_FRONT_MOTOR,
	(Motor_Id_t)BOARD_CAR_LEFT_REAR_MOTOR,
	(Motor_Id_t)BOARD_CAR_RIGHT_REAR_MOTOR
};

/* 电机编号到编码器编号的映射。
 * 闭环控制时，每个电机要读取与自己配对的编码器速度。 */
static const Encoder_Id_t g_motorEncoderMap[MOTOR_COUNT] =
{
	[BOARD_CAR_LEFT_FRONT_MOTOR] = (Encoder_Id_t)BOARD_CAR_LEFT_FRONT_ENCODER,
	[BOARD_CAR_RIGHT_FRONT_MOTOR] = (Encoder_Id_t)BOARD_CAR_RIGHT_FRONT_ENCODER,
	[BOARD_CAR_LEFT_REAR_MOTOR] = (Encoder_Id_t)BOARD_CAR_LEFT_REAR_ENCODER,
	[BOARD_CAR_RIGHT_REAR_MOTOR] = (Encoder_Id_t)BOARD_CAR_RIGHT_REAR_ENCODER
};

/* 根据 GPIO 端口实例打开对应的 GPIO 时钟。 */
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

/* 将输出命令限制在 PWM 能表达的范围内。 */
static int16_t Motor_LimitCommand(int16_t speed)
{
	int16_t maxDuty;

	maxDuty = (int16_t)PWM_GetMaxDuty();
	/* 上下限对称裁剪，保证既能控制正转也能控制反转。 */
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

/* 初始化单个方向控制引脚。 */
static void Motor_InitPin(GPIO_TypeDef *port, uint16_t pin)
{
	GPIO_InitTypeDef gpioInitStructure;

	Motor_GPIOClockCmd(port, ENABLE);

	/* 方向脚使用普通推挽输出即可。 */
	gpioInitStructure.GPIO_Pin = pin;
	gpioInitStructure.GPIO_Mode = GPIO_Mode_OUT;
	gpioInitStructure.GPIO_OType = GPIO_OType_PP;
	gpioInitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	gpioInitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(port, &gpioInitStructure);
}

/* 设置某个电机的方向引脚组合。 */
static void Motor_SetDirection(Motor_Id_t motor, BitAction in1Level, BitAction in2Level)
{
	if (motor >= MOTOR_COUNT)
	{
		return;
	}

	GPIO_WriteBit(g_motorConfig[motor].in1Port, g_motorConfig[motor].in1Pin, in1Level);
	GPIO_WriteBit(g_motorConfig[motor].in2Port, g_motorConfig[motor].in2Pin, in2Level);
}

/* 根据硬件反装标志修正控制方向。
 * 上层统一以“逻辑正转”为准，底层如果发现某电机安装反了，在这里取反即可。 */
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

/* 将一个带符号的速度命令真正作用到电机硬件。
 * 这里会同时处理：
 * - 占空比限幅
 * - 正反转方向控制
 * - STBY 唤醒
 * - 0 输出时的安全关闭 */
static void Motor_ApplyDriveCommand(Motor_Id_t motor, int16_t speed)
{
	int16_t actualSpeed;
	uint16_t duty;

	if (motor >= MOTOR_COUNT)
	{
		return;
	}

	/* 先限幅，再保存当前输出记录。 */
	speed = Motor_LimitCommand(speed);
	g_outputCommand[motor] = speed;

	/* 再根据硬件安装方向，转换成真实下发给驱动芯片的方向。 */
	actualSpeed = Motor_ApplyReverseFlag(motor, speed);

	if (actualSpeed > 0)
	{
		/* 正向转动：IN1=1, IN2=0，PWM 输出正占空比。 */
		Motor_SetStandby(ENABLE);
		Motor_SetDirection(motor, Bit_SET, Bit_RESET);
		PWM_SetDuty(g_motorConfig[motor].pwmChannel, (uint16_t)actualSpeed);
	}
	else if (actualSpeed < 0)
	{
		/* 反向转动：IN1=0, IN2=1，占空比取绝对值。 */
		Motor_SetStandby(ENABLE);
		Motor_SetDirection(motor, Bit_RESET, Bit_SET);
		duty = (uint16_t)(-actualSpeed);
		PWM_SetDuty(g_motorConfig[motor].pwmChannel, duty);
	}
	else
	{
		/* 速度为 0 时关闭 PWM，并将方向脚拉低，减少误动作。 */
		PWM_SetDuty(g_motorConfig[motor].pwmChannel, 0U);
		Motor_SetDirection(motor, Bit_RESET, Bit_RESET);
	}
}

/* 重置所有速度控制环的状态。 */
static void Motor_ResetControlLoops(void)
{
	uint8_t index;

	for (index = 0U; index < MOTOR_COUNT; index++)
	{
		/* PID 内部积分/微分状态和目标输出都回到初始值。 */
		PID_Reset(&g_speedPid[index]);
		g_targetSpeedRpm[index] = 0;
		g_outputCommand[index] = 0;
	}
}

/* 将最新的速度 PID 参数同步到所有电机控制器实例。 */
static void Motor_SyncPidProfiles(void)
{
	uint8_t index;

	for (index = 0U; index < MOTOR_COUNT; index++)
	{
		PID_ApplySpeedConfig(&g_speedPid[index]);
	}
}

/* 关闭闭环控制，但不直接决定整车状态。 */
static void Motor_DisableClosedLoop(void)
{
	g_closedLoopEnabled = 0U;
	Motor_ResetControlLoops();
}

/* 一次性设置左右两侧轮子的目标速度。 */
static void Motor_SetLeftRightTargets(int32_t leftTarget, int32_t rightTarget)
{
	g_targetSpeedRpm[g_carWheelMotorMap[CAR_WHEEL_LEFT_FRONT]] = leftTarget;
	g_targetSpeedRpm[g_carWheelMotorMap[CAR_WHEEL_LEFT_REAR]] = leftTarget;
	g_targetSpeedRpm[g_carWheelMotorMap[CAR_WHEEL_RIGHT_FRONT]] = rightTarget;
	g_targetSpeedRpm[g_carWheelMotorMap[CAR_WHEEL_RIGHT_REAR]] = rightTarget;
}

/* 根据当前整车状态，刷新四个轮子的目标速度。 */
static void Motor_UpdateTargetsByState(void)
{
	int32_t baseTarget;

	switch (g_carState)
	{
		case CAR_FORWARD:
			/* 前进时左右同速正转。 */
			baseTarget = g_moveTargetSpeedRpm;
			Motor_SetLeftRightTargets(baseTarget, baseTarget);
			break;

		case CAR_BACKWARD:
			/* 后退时左右同速反转。 */
			baseTarget = -g_moveTargetSpeedRpm;
			Motor_SetLeftRightTargets(baseTarget, baseTarget);
			break;

		case CAR_LEFT:
			/* 原地左转：左侧反转，右侧正转。 */
			Motor_SetLeftRightTargets(-g_turnTargetSpeedRpm, g_turnTargetSpeedRpm);
			return;

		case CAR_RIGHT:
			/* 原地右转：左侧正转，右侧反转。 */
			Motor_SetLeftRightTargets(g_turnTargetSpeedRpm, -g_turnTargetSpeedRpm);
			return;

		case CAR_STOP:
		default:
			/* 停止状态下四轮目标速度全部为 0。 */
			Motor_SetLeftRightTargets(0, 0);
			return;
	}
}

/* 进入某个闭环运动状态。 */
static void Motor_EnableClosedLoop(Motor_CarState_t state)
{
	g_carState = state;
	g_closedLoopEnabled = 1U;

	/* 每次切换状态时重置控制器，避免上一个状态残留积分影响新的动作。 */
	Motor_ResetControlLoops();
	Motor_SyncPidProfiles();

	/* 立即执行一次控制任务，减少状态切换后的响应延迟。 */
	Motor_ControlTask();
}

void Motor_Init(void)
{
	uint8_t index;

	/* 先初始化 PWM，再初始化所有方向控制引脚。 */
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

	/* 从配置区加载 PID 参数。 */
	PID_LoadSpeedConfig();

	for (index = 0U; index < MOTOR_COUNT; index++)
	{
		/* 每个电机各自初始化一套 PID 状态，并应用同一份速度参数。 */
		PID_Init(&g_speedPid[index]);
		PID_ApplySpeedConfig(&g_speedPid[index]);
		g_targetSpeedRpm[index] = 0;
		g_outputCommand[index] = 0;
	}

	/* 上电默认停车。 */
	Motor_CarStop();
}

void Motor_SetStandby(FunctionalState newState)
{
	/* STBY 为驱动芯片待机控制脚：
	 * ENABLE 表示退出待机，允许驱动输出；
	 * DISABLE 表示进入待机，整体关闭电机驱动。 */
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
	/* 单电机正转只是对 Motor_SetSpeed() 的语义封装。 */
	Motor_SetSpeed(motor, (int16_t)speed);
}

void Motor_Backward(Motor_Id_t motor, uint16_t speed)
{
	/* 单电机反转通过传递负值速度实现。 */
	Motor_SetSpeed(motor, (int16_t)(-((int16_t)speed)));
}

void Motor_Stop(Motor_Id_t motor)
{
	/* 停止单个电机时，既清目标也清输出。 */
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

	/* 所有电机先逐个停掉，再拉低 STBY，进入整机待机态。 */
	for (index = 0U; index < MOTOR_COUNT; index++)
	{
		Motor_Stop((Motor_Id_t)index);
	}

	Motor_SetStandby(DISABLE);
}

void Motor_SetSpeed(Motor_Id_t motor, int16_t speed)
{
	/* 这个接口是“直接驱动模式”，不会自动开启闭环。 */
	if (motor >= MOTOR_COUNT)
	{
		return;
	}

	Motor_ApplyDriveCommand(motor, speed);
}

void Motor_CarForward(uint16_t speed)
{
	/* 进入整车前进闭环状态前，先更新默认巡航目标速度。 */
	Motor_SetMoveTargetSpeedRpm((int32_t)speed);
	Motor_EnableClosedLoop(CAR_FORWARD);
}

void Motor_CarBackward(uint16_t speed)
{
	/* 后退与前进共用 moveTarget，只是方向不同。 */
	Motor_SetMoveTargetSpeedRpm((int32_t)speed);
	Motor_EnableClosedLoop(CAR_BACKWARD);
}

void Motor_CarTurnLeft(uint16_t speed)
{
	/* 转向单独使用 turnTarget。 */
	Motor_SetTurnTargetSpeedRpm((int32_t)speed);
	Motor_EnableClosedLoop(CAR_LEFT);
}

void Motor_CarTurnRight(uint16_t speed)
{
	/* 转向单独使用 turnTarget。 */
	Motor_SetTurnTargetSpeedRpm((int32_t)speed);
	Motor_EnableClosedLoop(CAR_RIGHT);
}

void Motor_CarStop(void)
{
	/* 停车时既退出闭环，也关闭全部电机输出。 */
	g_carState = CAR_STOP;
	Motor_DisableClosedLoop();
	Motor_StopAll();
}

void Motor_SetCarState(Motor_CarState_t state)
{
	/* 把外部给出的整车状态转换成具体动作入口。 */
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
	/* 返回当前整车运动状态。 */
	return g_carState;
}

Motor_CarState_t Motor_GetNextCarState(Motor_CarState_t state)
{
	/* 用于按键轮换测试：前进 -> 后退 -> 左转 -> 右转 -> 停止 -> 前进。 */
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
	/* 根据当前状态切换到下一个预定义状态。 */
	Motor_SetCarState(Motor_GetNextCarState(g_carState));
}

const char *Motor_GetCarStateName(Motor_CarState_t state)
{
	/* 返回用于打印/调试的状态字符串。 */
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

	/* 未开启闭环时，不参与速度调节。 */
	if (g_closedLoopEnabled == 0U)
	{
		return;
	}

	/* 先根据当前整车状态刷新 4 个轮子的目标速度。 */
	Motor_UpdateTargetsByState();

	for (index = 0U; index < MOTOR_COUNT; index++)
	{
		/* 实际速度来自与当前电机绑定的编码器。 */
		actualSpeed = Encoder_GetSpeedRpm(g_motorEncoderMap[index]);

		/* PID 输入：
		 * 目标值 = g_targetSpeedRpm[index]
		 * 反馈值 = actualSpeed
		 * 输出值 = PWM 占空比命令（带符号） */
		pwmCommand = (int16_t)PID_Calculate(&g_speedPid[index],
											(float)g_targetSpeedRpm[index],
											(float)actualSpeed);
		Motor_ApplyDriveCommand((Motor_Id_t)index, pwmCommand);
	}
}

void Motor_RefreshPidProfiles(void)
{
	/* 当 PID 参数在别处被修改后，可调用本函数让所有电机重新同步参数。 */
	Motor_SyncPidProfiles();
}

void Motor_SetMoveTargetSpeedRpm(int32_t speedRpm)
{
	/* 直行目标速度只保留幅值，不保留方向符号。 */
	if (speedRpm < 0)
	{
		speedRpm = -speedRpm;
	}

	/* 限制在板级配置允许的最大范围内。 */
	if (speedRpm > BOARD_CAR_MOVE_TARGET_SPEED_MAX_RPM)
	{
		speedRpm = BOARD_CAR_MOVE_TARGET_SPEED_MAX_RPM;
	}

	g_moveTargetSpeedRpm = speedRpm;
}

void Motor_SetTurnTargetSpeedRpm(int32_t speedRpm)
{
	/* 转向目标速度同样只保留幅值。 */
	if (speedRpm < 0)
	{
		speedRpm = -speedRpm;
	}

	/* 限制在板级配置允许的最大范围内。 */
	if (speedRpm > BOARD_CAR_TURN_TARGET_SPEED_MAX_RPM)
	{
		speedRpm = BOARD_CAR_TURN_TARGET_SPEED_MAX_RPM;
	}

	g_turnTargetSpeedRpm = speedRpm;
}

void Motor_SetHeadingAssistEnabled(uint8_t enable)
{
	/* 预留接口：当前工程暂未实现航向辅助，因此仅消除未使用参数告警。 */
	(void)enable;
}

int32_t Motor_GetMoveTargetSpeedRpm(void)
{
	/* 获取当前保存的默认直行目标速度。 */
	return g_moveTargetSpeedRpm;
}

int32_t Motor_GetTurnTargetSpeedRpm(void)
{
	/* 获取当前保存的默认转向目标速度。 */
	return g_turnTargetSpeedRpm;
}

int32_t Motor_GetTargetSpeedRpm(Motor_Id_t motor)
{
	/* 查询指定电机当前的闭环目标速度。 */
	if (motor >= MOTOR_COUNT)
	{
		return 0;
	}

	return g_targetSpeedRpm[motor];
}

int32_t Motor_GetActualSpeedRpm(Motor_Id_t motor)
{
	/* 查询指定电机的编码器反馈速度。 */
	if (motor >= MOTOR_COUNT)
	{
		return 0;
	}

	return Encoder_GetSpeedRpm(g_motorEncoderMap[motor]);
}

int16_t Motor_GetOutputCommand(Motor_Id_t motor)
{
	/* 查询指定电机最近一次下发的控制命令。 */
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

	/* 把当前输出命令换算成百分比，便于上位机显示。 */
	if (motor >= MOTOR_COUNT)
	{
		return 0;
	}

	maxDuty = (int32_t)PWM_GetMaxDuty();
	if (maxDuty <= 0)
	{
		return 0;
	}

	/* 四舍五入换算到 -100% ~ 100% 左右的区间。 */
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
	/* 预留接口：当前未实现航向修正。 */
	return 0.0f;
}

uint8_t Motor_IsHeadingCorrectionEnabled(void)
{
	/* 预留接口：当前未实现航向修正。 */
	return 0U;
}

uint8_t Motor_IsHeadingAssistEnabled(void)
{
	/* 预留接口：当前未实现航向辅助。 */
	return 0U;
}

uint8_t Motor_IsClosedLoopEnabled(void)
{
	/* 返回当前是否处于闭环调速状态。 */
	return g_closedLoopEnabled;
}
