#include "AppTask.h"
#include "BlueSerial.h"
#include "BoardConfig.h"
#include "HMC5883L.h"
#include "IMU_Fusion.h"
#include "Key.h"
#include "MPU6050.h"
#include "Motor.h"
#include "PID.h"
#include "Serial.h"
#include "Timer.h"

#define APP_TASK_SLIDER_REPLY_DELAY_MS           150U
#define APP_TASK_SLIDER_REPLY_NONE               0U
#define APP_TASK_SLIDER_REPLY_PID                1U
#define APP_TASK_SLIDER_REPLY_CONFIG             2U

static uint8_t g_streamEnabled = 0U;
static uint32_t g_lastTelemetryTick = 0U;
static uint8_t g_sliderReplyPending = APP_TASK_SLIDER_REPLY_NONE;
static uint32_t g_lastSliderUpdateTick = 0U;

static char AppTask_ToLowerAscii(char ch)
{
	if ((ch >= 'A') && (ch <= 'Z'))
	{
		return (char)(ch + ('a' - 'A'));
	}

	return ch;
}

static uint32_t AppTask_Pow10(uint8_t exponent)
{
	uint32_t result;
	uint8_t index;

	result = 1U;
	for (index = 0U; index < exponent; index++)
	{
		result *= 10U;
	}

	return result;
}

static void AppTask_SendUnsignedRaw(uint32_t value, uint8_t minWidth)
{
	char digits[10];
	uint8_t digitCount;

	digitCount = 0U;
	if (value == 0U)
	{
		digits[digitCount] = '0';
		digitCount++;
	}
	else
	{
		while ((value > 0U) && (digitCount < (uint8_t)sizeof(digits)))
		{
			digits[digitCount] = (char)('0' + (value % 10U));
			digitCount++;
			value /= 10U;
		}
	}

	while (digitCount < minWidth)
	{
		digits[digitCount] = '0';
		digitCount++;
	}

	while (digitCount > 0U)
	{
		digitCount--;
		BlueSerial_SendByte((uint8_t)digits[digitCount]);
	}
}

static uint8_t AppTask_StringEqualsIgnoreCase(const char *left, const char *right)
{
	if ((left == 0) || (right == 0))
	{
		return 0U;
	}

	while ((*left != '\0') && (*right != '\0'))
	{
		if (AppTask_ToLowerAscii(*left) != AppTask_ToLowerAscii(*right))
		{
			return 0U;
		}

		left++;
		right++;
	}

	return (uint8_t)((*left == '\0') && (*right == '\0'));
}

static uint8_t AppTask_IsPidSliderName(const char *name, char axis)
{
	if (name == 0)
	{
		return 0U;
	}

	switch (axis)
	{
		case 'P':
			return (uint8_t)(AppTask_StringEqualsIgnoreCase(name, "P") ||
							  AppTask_StringEqualsIgnoreCase(name, "KP"));

		case 'I':
			return (uint8_t)(AppTask_StringEqualsIgnoreCase(name, "I") ||
							  AppTask_StringEqualsIgnoreCase(name, "KI"));

		case 'D':
			return (uint8_t)(AppTask_StringEqualsIgnoreCase(name, "D") ||
							  AppTask_StringEqualsIgnoreCase(name, "KD"));

		default:
			return 0U;
	}
}

static uint8_t AppTask_IsMoveTargetSliderName(const char *name)
{
	if (name == 0)
	{
		return 0U;
	}

	return (uint8_t)(AppTask_StringEqualsIgnoreCase(name, "T") ||
					  AppTask_StringEqualsIgnoreCase(name, "TARGET"));
}

static uint8_t AppTask_TrySetCarStateByName(const char *name)
{
	if (AppTask_StringEqualsIgnoreCase(name, "FORWARD"))
	{
		Motor_SetCarState(CAR_FORWARD);
	}
	else if (AppTask_StringEqualsIgnoreCase(name, "BACKWARD"))
	{
		Motor_SetCarState(CAR_BACKWARD);
	}
	else if (AppTask_StringEqualsIgnoreCase(name, "LEFT"))
	{
		Motor_SetCarState(CAR_LEFT);
	}
	else if (AppTask_StringEqualsIgnoreCase(name, "RIGHT"))
	{
		Motor_SetCarState(CAR_RIGHT);
	}
	else if (AppTask_StringEqualsIgnoreCase(name, "STOP"))
	{
		Motor_SetCarState(CAR_STOP);
	}
	else
	{
		return 1U;
	}

	return 0U;
}

static int32_t AppTask_RoundFloatToInt32(float value)
{
	if (value >= 0.0f)
	{
		return (int32_t)(value + 0.5f);
	}

	return (int32_t)(value - 0.5f);
}

static uint8_t AppTask_ParseInt32(const char *text, int32_t *value)
{
	uint8_t negative;
	uint8_t hasDigit;
	uint32_t parsedValue;
	uint32_t limit;
	uint32_t digit;

	if ((text == 0) || (value == 0))
	{
		return 1U;
	}

	negative = 0U;
	if (*text == '-')
	{
		negative = 1U;
		text++;
	}
	else if (*text == '+')
	{
		text++;
	}

	limit = (negative != 0U) ? 2147483648UL : 2147483647UL;
	hasDigit = 0U;
	parsedValue = 0U;
	while ((*text >= '0') && (*text <= '9'))
	{
		hasDigit = 1U;
		digit = (uint32_t)(*text - '0');
		if (parsedValue > ((limit - digit) / 10U))
		{
			return 1U;
		}

		parsedValue = (parsedValue * 10U) + digit;
		text++;
	}

	if ((hasDigit == 0U) || (*text != '\0'))
	{
		return 1U;
	}

	if (negative != 0U)
	{
		if (parsedValue == 2147483648UL)
		{
			*value = (int32_t)0x80000000UL;
		}
		else
		{
			*value = -(int32_t)parsedValue;
		}
	}
	else
	{
		*value = (int32_t)parsedValue;
	}
	return 0U;
}

static uint8_t AppTask_ParseFloat(const char *text, float *value)
{
	uint8_t negative;
	uint8_t hasDigit;
	float parsedValue;
	float fractionScale;

	if ((text == 0) || (value == 0))
	{
		return 1U;
	}

	negative = 0U;
	if (*text == '-')
	{
		negative = 1U;
		text++;
	}
	else if (*text == '+')
	{
		text++;
	}

	hasDigit = 0U;
	parsedValue = 0.0f;
	while ((*text >= '0') && (*text <= '9'))
	{
		hasDigit = 1U;
		parsedValue = (parsedValue * 10.0f) + (float)(*text - '0');
		text++;
	}

	if (*text == '.')
	{
		text++;
		fractionScale = 0.1f;
		while ((*text >= '0') && (*text <= '9'))
		{
			hasDigit = 1U;
			parsedValue += (float)(*text - '0') * fractionScale;
			fractionScale *= 0.1f;
			text++;
		}
	}

	if ((hasDigit == 0U) || (*text != '\0'))
	{
		return 1U;
	}

	*value = (negative != 0U) ? (-parsedValue) : parsedValue;
	return 0U;
}

static void AppTask_SendOk(const char *module, const char *action)
{
	BlueSerial_SendString("[OK,");
	BlueSerial_SendString(module);
	BlueSerial_SendByte(',');
	BlueSerial_SendString(action);
	BlueSerial_SendString("]\r\n");
}

static void AppTask_SendError(const char *reason)
{
	BlueSerial_SendString("[ERR,");
	BlueSerial_SendString(reason);
	BlueSerial_SendString("]\r\n");
}

static void AppTask_SendUnsignedValue(uint32_t value)
{
	AppTask_SendUnsignedRaw(value, 1U);
}

static void AppTask_SendSignedValue(int32_t value)
{
	uint32_t absoluteValue;

	if (value < 0)
	{
		BlueSerial_SendByte((uint8_t)'-');
		absoluteValue = (uint32_t)(-(value + 1));
		absoluteValue += 1U;
	}
	else
	{
		absoluteValue = (uint32_t)value;
	}

	AppTask_SendUnsignedRaw(absoluteValue, 1U);
}

static void AppTask_SendFixedValue(float value, uint8_t decimals)
{
	int32_t scaledValue;
	uint32_t absoluteScaledValue;
	uint32_t factor;
	uint32_t integerPart;
	uint32_t fractionalPart;

	factor = AppTask_Pow10(decimals);

	if (value >= 0.0f)
	{
		scaledValue = (int32_t)((value * (float)factor) + 0.5f);
	}
	else
	{
		scaledValue = (int32_t)((value * (float)factor) - 0.5f);
	}

	if (scaledValue < 0)
	{
		absoluteScaledValue = (uint32_t)(-scaledValue);
	}
	else
	{
		absoluteScaledValue = (uint32_t)scaledValue;
	}

	integerPart = absoluteScaledValue / factor;
	fractionalPart = absoluteScaledValue % factor;

	if (scaledValue < 0)
	{
		BlueSerial_SendByte((uint8_t)'-');
	}

	AppTask_SendUnsignedRaw(integerPart, 1U);
	if (decimals > 0U)
	{
		BlueSerial_SendByte((uint8_t)'.');
		AppTask_SendUnsignedRaw(fractionalPart, decimals);
	}
}

static void AppTask_SendPidProfile(const char *name)
{
	PID_SpeedConfig_t profile;

	PID_GetSpeedConfig(&profile);
	BlueSerial_SendString("[PID,");
	BlueSerial_SendString(name);
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(profile.kp, 4U);
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(profile.ki, 4U);
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(profile.kd, 4U);
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(profile.integralLimit, 2U);
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(profile.outputLimit, 2U);
	BlueSerial_SendString("]\r\n");
}

static void AppTask_SendConfigFrame(void)
{
	PID_SpeedConfig_t profile;

	PID_GetSpeedConfig(&profile);
	BlueSerial_SendString("[CFG,");
	AppTask_SendSignedValue(Motor_GetMoveTargetSpeedRpm());
	BlueSerial_SendByte(',');
	AppTask_SendSignedValue(Motor_GetTurnTargetSpeedRpm());
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(profile.outputLimit, 2U);
	BlueSerial_SendByte(',');
	AppTask_SendUnsignedValue((uint32_t)BOARD_BLUETOOTH_TELEMETRY_MS);
	BlueSerial_SendString("]\r\n");
}

static void AppTask_SendStateFrame(void)
{
	BlueSerial_SendString("[STATE,");
	BlueSerial_SendString(Motor_GetCarStateName(Motor_GetCarState()));
	BlueSerial_SendByte(',');
	AppTask_SendUnsignedValue((uint32_t)Motor_IsClosedLoopEnabled());
	BlueSerial_SendByte(',');
	AppTask_SendUnsignedValue((uint32_t)Motor_IsHeadingCorrectionEnabled());
	BlueSerial_SendString("]\r\n");
}

static void AppTask_SendMpuFrame(void)
{
	BlueSerial_SendString("[MPU,");
	AppTask_SendUnsignedValue((uint32_t)MPU6050_IsOnline());
	BlueSerial_SendByte(',');
	AppTask_SendUnsignedValue((uint32_t)MPU6050_IsCalibrated());
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(MPU6050_GetGyroZDps(), 2U);
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(MPU6050_GetYawDeg(), 2U);
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(MPU6050_GetGyroBiasZDps(), 2U);
	BlueSerial_SendString("]\r\n");
}

static void AppTask_SendImuDiagFrame(void)
{
	BlueSerial_SendString("[IMU,");
	AppTask_SendFixedValue(MPU6050_GetPitchAccDeg(), 2U);
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(MPU6050_GetRollAccDeg(), 2U);
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(MPU6050_GetPitchTiltDeg(), 2U);
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(MPU6050_GetRollTiltDeg(), 2U);
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(MPU6050_GetAccelNormG(), 3U);
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(MPU6050_GetAccelYG(), 3U);
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(HMC5883L_GetFieldY(), 4U);
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(IMU_Fusion_GetMagHeadingRawDeg(), 2U);
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(IMU_Fusion_GetMagHeadingTiltCompDeg(), 2U);
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(IMU_Fusion_GetMagHeadingDeg(), 2U);
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(MPU6050_GetYawGyroOnlyDeg(), 2U);
	BlueSerial_SendByte(',');
	AppTask_SendFixedValue(MPU6050_GetYawDeg(), 2U);
	BlueSerial_SendByte(',');
	AppTask_SendUnsignedValue((uint32_t)IMU_Fusion_IsMagCorrectionValid());
	BlueSerial_SendByte(',');
	AppTask_SendUnsignedValue((uint32_t)MPU6050_IsAccelTiltValid());
	BlueSerial_SendByte(',');
	AppTask_SendUnsignedValue((uint32_t)IMU_Fusion_IsMagRecoveryPending());
	BlueSerial_SendString("]\r\n");
}

static void AppTask_SendPlotFrame(void)
{
	Motor_Id_t leftFrontMotor;

	leftFrontMotor = (Motor_Id_t)BOARD_CAR_LEFT_FRONT_MOTOR;
	BlueSerial_SendString("[plot,");
	AppTask_SendSignedValue(Motor_GetTargetSpeedRpm(leftFrontMotor));
	BlueSerial_SendByte(',');
	AppTask_SendSignedValue(Motor_GetActualSpeedRpm(leftFrontMotor));
	BlueSerial_SendByte(',');
	AppTask_SendSignedValue((int32_t)Motor_GetOutputCommand(leftFrontMotor));
	BlueSerial_SendString("]\r\n");
}

static void AppTask_HandlePidCommand(uint8_t fieldCount)
{
	float kp;
	float ki;
	float kd;
	float integralLimit;
	float outputLimit;

	if ((fieldCount == 2U) && AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[1], "GET"))
	{
		AppTask_SendPidProfile("SPEED");
		return;
	}

	if ((fieldCount == 6U) && AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[1], "SET"))
	{
		if ((AppTask_ParseFloat(BlueSerial_StringArray[3], &kp) != 0U) ||
			(AppTask_ParseFloat(BlueSerial_StringArray[4], &ki) != 0U) ||
			(AppTask_ParseFloat(BlueSerial_StringArray[5], &kd) != 0U))
		{
			AppTask_SendError("BAD_NUMBER");
			return;
		}

		if (AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[2], "SPEED"))
		{
			PID_SetSpeedTunings(kp, ki, kd);
			Motor_RefreshPidProfiles();
			AppTask_SendOk("PID", "SET_SPEED");
			AppTask_SendPidProfile("SPEED");
			return;
		}
	}

	if ((fieldCount == 5U) && AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[1], "LIMIT"))
	{
		if ((AppTask_ParseFloat(BlueSerial_StringArray[3], &integralLimit) != 0U) ||
			(AppTask_ParseFloat(BlueSerial_StringArray[4], &outputLimit) != 0U))
		{
			AppTask_SendError("BAD_NUMBER");
			return;
		}

		if (AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[2], "SPEED"))
		{
			if (outputLimit < 1.0f)
			{
				outputLimit = 1.0f;
			}
			if (outputLimit > (float)BOARD_PWM_MAX_DUTY)
			{
				outputLimit = (float)BOARD_PWM_MAX_DUTY;
			}

			PID_SetSpeedLimits(integralLimit, outputLimit);
			Motor_RefreshPidProfiles();
			AppTask_SendOk("PID", "LIMIT_SPEED");
			AppTask_SendPidProfile("SPEED");
			return;
		}
	}

	AppTask_SendError("PID_CMD");
}

static void AppTask_HandleSliderCommand(uint8_t fieldCount)
{
	float value;
	PID_SpeedConfig_t profile;
	int32_t targetSpeedRpm;

	if (fieldCount != 3U)
	{
		AppTask_SendError("SLIDER_CMD");
		return;
	}

	if (AppTask_ParseFloat(BlueSerial_StringArray[2], &value) != 0U)
	{
		AppTask_SendError("BAD_NUMBER");
		return;
	}

	if (AppTask_IsMoveTargetSliderName(BlueSerial_StringArray[1]) != 0U)
	{
		targetSpeedRpm = AppTask_RoundFloatToInt32(value);
		if (targetSpeedRpm > BOARD_CAR_MOVE_TARGET_SPEED_MAX_RPM)
		{
			targetSpeedRpm = BOARD_CAR_MOVE_TARGET_SPEED_MAX_RPM;
		}
		if (targetSpeedRpm < -BOARD_CAR_MOVE_TARGET_SPEED_MAX_RPM)
		{
			targetSpeedRpm = -BOARD_CAR_MOVE_TARGET_SPEED_MAX_RPM;
		}

		Motor_SetMoveTargetSpeedRpm(targetSpeedRpm);
		g_sliderReplyPending = APP_TASK_SLIDER_REPLY_CONFIG;
		g_lastSliderUpdateTick = Timer_GetTick();
		return;
	}

	PID_GetSpeedConfig(&profile);
	if (AppTask_IsPidSliderName(BlueSerial_StringArray[1], 'P') != 0U)
	{
		profile.kp = value;
	}
	else if (AppTask_IsPidSliderName(BlueSerial_StringArray[1], 'I') != 0U)
	{
		profile.ki = value;
	}
	else if (AppTask_IsPidSliderName(BlueSerial_StringArray[1], 'D') != 0U)
	{
		profile.kd = value;
	}
	else
	{
		AppTask_SendError("SLIDER_NAME");
		return;
	}

	PID_SetSpeedTunings(profile.kp, profile.ki, profile.kd);
	Motor_RefreshPidProfiles();
	g_sliderReplyPending = APP_TASK_SLIDER_REPLY_PID;
	g_lastSliderUpdateTick = Timer_GetTick();
}

static void AppTask_HandleCtrlCommand(uint8_t fieldCount)
{
	int32_t value;
	PID_SpeedConfig_t profile;

	if (fieldCount != 3U)
	{
		AppTask_SendError("CTRL_CMD");
		return;
	}

	if (AppTask_ParseInt32(BlueSerial_StringArray[2], &value) != 0U)
	{
		AppTask_SendError("BAD_NUMBER");
		return;
	}

	if (AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[1], "SPEED"))
	{
		Motor_SetMoveTargetSpeedRpm(value);
		AppTask_SendOk("CTRL", "SPEED");
		AppTask_SendConfigFrame();
		return;
	}

	if (AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[1], "TURN"))
	{
		Motor_SetTurnTargetSpeedRpm(value);
		AppTask_SendOk("CTRL", "TURN");
		AppTask_SendConfigFrame();
		return;
	}

	if (AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[1], "HEADING"))
	{
		Motor_SetHeadingAssistEnabled((uint8_t)(value != 0));
		AppTask_SendOk("CTRL", "HEADING");
		AppTask_SendStateFrame();
		return;
	}

	if (AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[1], "PWM"))
	{
		if (value < 1)
		{
			value = 1;
		}
		if (value > (int32_t)BOARD_PWM_MAX_DUTY)
		{
			value = BOARD_PWM_MAX_DUTY;
		}

		PID_GetSpeedConfig(&profile);
		PID_SetSpeedLimits(profile.integralLimit, (float)value);
		Motor_RefreshPidProfiles();
		AppTask_SendOk("CTRL", "PWM");
		AppTask_SendPidProfile("SPEED");
		return;
	}

	AppTask_SendError("CTRL_CMD");
}

static void AppTask_HandleStateCommand(uint8_t fieldCount)
{
	if ((fieldCount == 2U) && AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[1], "GET"))
	{
		AppTask_SendStateFrame();
		return;
	}

	if ((fieldCount == 3U) && AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[1], "SET"))
	{
		if (AppTask_TrySetCarStateByName(BlueSerial_StringArray[2]) != 0U)
		{
			AppTask_SendError("STATE_CMD");
			return;
		}

		AppTask_SendOk("STATE", "SET");
		AppTask_SendStateFrame();
		return;
	}

	AppTask_SendError("STATE_CMD");
}

static void AppTask_HandleModeCommand(uint8_t fieldCount)
{
	if (fieldCount != 2U)
	{
		AppTask_SendError("MODE_CMD");
		return;
	}

	if (AppTask_TrySetCarStateByName(BlueSerial_StringArray[1]) != 0U)
	{
		AppTask_SendError("MODE_CMD");
		return;
	}

	AppTask_SendOk("MODE", Motor_GetCarStateName(Motor_GetCarState()));
	AppTask_SendStateFrame();
}

static void AppTask_HandleMpuCommand(uint8_t fieldCount)
{
	if ((fieldCount == 2U) && AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[1], "GET"))
	{
		AppTask_SendMpuFrame();
		return;
	}

	if ((fieldCount == 2U) && AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[1], "ZERO"))
	{
		MPU6050_StartGyroBiasCalibration();
		AppTask_SendOk("MPU", "ZERO");
		AppTask_SendMpuFrame();
		return;
	}

	AppTask_SendError("MPU_CMD");
}

static void AppTask_HandleImuCommand(uint8_t fieldCount)
{
	if ((fieldCount == 2U) && AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[1], "GET"))
	{
		AppTask_SendImuDiagFrame();
		return;
	}

	AppTask_SendError("IMU_CMD");
}

static void AppTask_HandleTelemetryCommand(uint8_t fieldCount)
{
	if ((fieldCount == 2U) && AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[1], "GET"))
	{
		AppTask_SendPlotFrame();
		return;
	}

	if ((fieldCount == 2U) && AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[1], "ON"))
	{
		g_streamEnabled = 1U;
		g_lastTelemetryTick = Timer_GetTick();
		AppTask_SendOk("TEL", "ON");
		return;
	}

	if ((fieldCount == 2U) && AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[1], "OFF"))
	{
		g_streamEnabled = 0U;
		AppTask_SendOk("TEL", "OFF");
		return;
	}

	AppTask_SendError("TEL_CMD");
}

static void AppTask_DispatchBluetoothFrame(void)
{
	uint8_t fieldCount;

	fieldCount = BlueSerial_GetFieldCount();
	if (fieldCount == 0U)
	{
		return;
	}

	if (AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[0], "PID"))
	{
		AppTask_HandlePidCommand(fieldCount);
	}
	else if (AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[0], "CTRL"))
	{
		AppTask_HandleCtrlCommand(fieldCount);
	}
	else if (AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[0], "STATE"))
	{
		AppTask_HandleStateCommand(fieldCount);
	}
	else if (AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[0], "MODE"))
	{
		AppTask_HandleModeCommand(fieldCount);
	}
	else if (AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[0], "MPU"))
	{
		AppTask_HandleMpuCommand(fieldCount);
	}
	else if (AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[0], "IMU"))
	{
		AppTask_HandleImuCommand(fieldCount);
	}
	else if (AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[0], "TEL"))
	{
		AppTask_HandleTelemetryCommand(fieldCount);
	}
	else if (AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[0], "CFG"))
	{
		AppTask_SendConfigFrame();
	}
	else if (AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[0], "slider") ||
			 AppTask_StringEqualsIgnoreCase(BlueSerial_StringArray[0], "s"))
	{
		AppTask_HandleSliderCommand(fieldCount);
	}
	else
	{
		AppTask_SendError("BAD_CMD");
	}
}

static void AppTask_ProcessKey(void)
{
	if (Key_Scan() != 0U)
	{
		Motor_SwitchToNextState();
		AppTask_SendStateFrame();
	}
}

static void AppTask_ProcessBluetooth(void)
{
	if (BlueSerial_ReceiveFlag() != 0U)
	{
		BlueSerial_Receive();
		AppTask_DispatchBluetoothFrame();
	}
}

static void AppTask_ProcessDeferredReplies(void)
{
	uint32_t nowTick;

	if (g_sliderReplyPending == APP_TASK_SLIDER_REPLY_NONE)
	{
		return;
	}

	nowTick = Timer_GetTick();
	if ((nowTick - g_lastSliderUpdateTick) < APP_TASK_SLIDER_REPLY_DELAY_MS)
	{
		return;
	}

	if (g_sliderReplyPending == APP_TASK_SLIDER_REPLY_PID)
	{
		AppTask_SendOk("SLIDER", "PID_SPEED");
		AppTask_SendPidProfile("SPEED");
	}
	else if (g_sliderReplyPending == APP_TASK_SLIDER_REPLY_CONFIG)
	{
		AppTask_SendOk("SLIDER", "TARGET_SPEED");
		AppTask_SendConfigFrame();
	}

	g_sliderReplyPending = APP_TASK_SLIDER_REPLY_NONE;
}

static void AppTask_ProcessTelemetry(void)
{
	uint32_t nowTick;

	if (g_streamEnabled == 0U)
	{
		return;
	}

	nowTick = Timer_GetTick();
	if ((nowTick - g_lastTelemetryTick) >= BOARD_BLUETOOTH_TELEMETRY_MS)
	{
		g_lastTelemetryTick = nowTick;
		AppTask_SendPlotFrame();
	}
}

void AppTask_Init(void)
{
	Serial_Init();
	BlueSerial_Init();

	g_streamEnabled = BOARD_BLUETOOTH_DEFAULT_STREAM_ENABLE;
	g_lastTelemetryTick = Timer_GetTick();
	g_sliderReplyPending = APP_TASK_SLIDER_REPLY_NONE;
	g_lastSliderUpdateTick = g_lastTelemetryTick;
	AppTask_SendStateFrame();
	AppTask_SendConfigFrame();
	AppTask_SendMpuFrame();
}

void AppTask_Run(void)
{
	AppTask_ProcessKey();
	AppTask_ProcessBluetooth();
	AppTask_ProcessDeferredReplies();
	AppTask_ProcessTelemetry();
}
