#include "PID.h"
#include "BoardConfig.h"

static PID_SpeedConfig_t g_speedPidConfig;

static float PID_Abs(float value)
{
	if (value < 0.0f)
	{
		return -value;
	}

	return value;
}

static float PID_Clamp(float value, float minValue, float maxValue)
{
	if (value > maxValue)
	{
		return maxValue;
	}

	if (value < minValue)
	{
		return minValue;
	}

	return value;
}

static float PID_GetSafeOutputLimit(float outputLimit)
{
	outputLimit = PID_Abs(outputLimit);
	if (outputLimit < 1.0f)
	{
		outputLimit = 1.0f;
	}

	return outputLimit;
}

void PID_LoadSpeedConfig(void)
{
	g_speedPidConfig.kp = BOARD_SPEED_PID_KP;
	g_speedPidConfig.ki = BOARD_SPEED_PID_KI;
	g_speedPidConfig.kd = BOARD_SPEED_PID_KD;
	g_speedPidConfig.integralLimit = PID_Abs(BOARD_SPEED_PID_INTEGRAL_LIMIT);
	g_speedPidConfig.outputLimit = PID_GetSafeOutputLimit((float)BOARD_SPEED_PID_OUTPUT_LIMIT);
	g_speedPidConfig.outputOffset = 0.0f;
}

void PID_GetSpeedConfig(PID_SpeedConfig_t *config)
{
	if (config == 0)
	{
		return;
	}

	*config = g_speedPidConfig;
}

void PID_SetSpeedTunings(float kp, float ki, float kd)
{
	g_speedPidConfig.kp = kp;
	g_speedPidConfig.ki = ki;
	g_speedPidConfig.kd = kd;
}

void PID_SetSpeedLimits(float integralLimit, float outputLimit)
{
	g_speedPidConfig.integralLimit = PID_Abs(integralLimit);
	g_speedPidConfig.outputLimit = PID_GetSafeOutputLimit(outputLimit);
}

void PID_Init(PID_t *p)
{
	if (p == 0)
	{
		return;
	}

	p->Target = 0.0f;
	p->Actual = 0.0f;
	p->Actual1 = 0.0f;
	p->Out = 0.0f;
	p->Kp = 0.0f;
	p->Ki = 0.0f;
	p->Kd = 0.0f;
	p->Error0 = 0.0f;
	p->Error1 = 0.0f;
	p->POut = 0.0f;
	p->IOut = 0.0f;
	p->DOut = 0.0f;
	p->IOutMax = 0.0f;
	p->IOutMin = 0.0f;
	p->OutMax = 0.0f;
	p->OutMin = 0.0f;
	p->OutOffset = 0.0f;
}

void PID_Reset(PID_t *p)
{
	if (p == 0)
	{
		return;
	}

	p->Target = 0.0f;
	p->Actual = 0.0f;
	p->Actual1 = 0.0f;
	p->Out = 0.0f;
	p->Error0 = 0.0f;
	p->Error1 = 0.0f;
	p->POut = 0.0f;
	p->IOut = 0.0f;
	p->DOut = 0.0f;
}

void PID_ApplySpeedConfig(PID_t *p)
{
	float integralLimit;
	float outputLimit;

	if (p == 0)
	{
		return;
	}

	integralLimit = PID_Abs(g_speedPidConfig.integralLimit);
	outputLimit = PID_GetSafeOutputLimit(g_speedPidConfig.outputLimit);

	p->Kp = g_speedPidConfig.kp;
	p->Ki = g_speedPidConfig.ki;
	p->Kd = g_speedPidConfig.kd;
	p->IOutMax = integralLimit;
	p->IOutMin = -integralLimit;
	p->OutMax = outputLimit;
	p->OutMin = -outputLimit;
	p->OutOffset = PID_Abs(g_speedPidConfig.outputOffset);
	p->IOut = PID_Clamp(p->IOut, p->IOutMin, p->IOutMax);
	p->Out = PID_Clamp(p->Out, p->OutMin, p->OutMax);
}

void PID_Update(PID_t *p)
{
	if (p == 0)
	{
		return;
	}

	p->Error1 = p->Error0;
	p->Error0 = p->Target - p->Actual;

	p->POut = p->Kp * p->Error0;

	if (p->Ki != 0.0f)
	{
		p->IOut += p->Ki * p->Error0;
	}
	else
	{
		p->IOut = 0.0f;
	}

	p->IOut = PID_Clamp(p->IOut, p->IOutMin, p->IOutMax);
	p->DOut = -p->Kd * (p->Actual - p->Actual1);

	p->Out = p->POut + p->IOut + p->DOut;

	if (p->Out > 0.0f)
	{
		p->Out += p->OutOffset;
	}
	else if (p->Out < 0.0f)
	{
		p->Out -= p->OutOffset;
	}

	p->Out = PID_Clamp(p->Out, p->OutMin, p->OutMax);
	p->Actual1 = p->Actual;
}

float PID_Calculate(PID_t *p, float target, float actual)
{
	if (p == 0)
	{
		return 0.0f;
	}

	p->Target = target;
	p->Actual = actual;
	PID_Update(p);
	return p->Out;
}
