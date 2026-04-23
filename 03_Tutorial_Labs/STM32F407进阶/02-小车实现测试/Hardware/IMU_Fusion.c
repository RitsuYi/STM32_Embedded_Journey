#include "IMU_Fusion.h"
#include "BoardConfig.h"
#include "HMC5883L.h"
#include "MPU6050.h"
#include <math.h>

#define IMU_FUSION_DEG_TO_RAD                  0.01745329252f
#define IMU_FUSION_SAMPLE_DT_S                 ((float)BOARD_MPU6050_SAMPLE_MS / 1000.0f)

static volatile float g_fusedYawDeg = 0.0f;
static volatile float g_magHeadingDeg = 0.0f;
static volatile float g_magHeadingRawDeg = 0.0f;
static volatile float g_magHeadingTiltCompDeg = 0.0f;
static volatile float g_gyroHeadingDeg = 0.0f;
static volatile uint8_t g_magHeadingReady = 0U;
static volatile uint8_t g_magCorrectionValid = 0U;
static volatile uint16_t g_magRecoveryStableMs = 0U;
static volatile uint8_t g_magRecoveryPending = 0U;
static volatile uint8_t g_magRecoveryBoostActive = 0U;

static float Angle_Wrap180(float angle)
{
	while (angle > 180.0f)
	{
		angle -= 360.0f;
	}

	while (angle < -180.0f)
	{
		angle += 360.0f;
	}

	return angle;
}

static float Angle_DiffDeg(float target, float current)
{
	return Angle_Wrap180(target - current);
}

static float IMU_Fusion_ComputeTiltCompensatedHeadingDeg(float mx,
														 float my,
														 float mz,
														 float pitchDeg,
														 float rollDeg)
{
	float pitchRad;
	float rollRad;
	float xh;
	float yh;
	float headingDeg;

	pitchRad = pitchDeg * IMU_FUSION_DEG_TO_RAD;
	rollRad = rollDeg * IMU_FUSION_DEG_TO_RAD;

	xh = (mx * cosf(pitchRad)) + (mz * sinf(pitchRad));
	yh = (mx * sinf(rollRad) * sinf(pitchRad)) +
		 (my * cosf(rollRad)) -
		 (mz * sinf(rollRad) * cosf(pitchRad));

	headingDeg = atan2f(yh, xh) * 57.2957795f;
	headingDeg += BOARD_HMC5883L_HEADING_DECLINATION_DEG;
	return Angle_Wrap180(headingDeg);
}

static uint8_t IMU_Fusion_IsTiltCompensationUsable(float pitchDeg, float rollDeg, float accNormG)
{
	if (MPU6050_IsAccelTiltValid() == 0U)
	{
		return 0U;
	}

	if ((fabsf(pitchDeg) > BOARD_YAW_MAG_MAX_TILT_DEG) ||
		(fabsf(rollDeg) > BOARD_YAW_MAG_MAX_TILT_DEG))
	{
		return 0U;
	}

	if ((BOARD_YAW_MAG_REJECT_ON_BAD_ACCEL != 0U) &&
		((accNormG < BOARD_MPU6050_ACCEL_NORM_MIN_G) ||
		 (accNormG > BOARD_MPU6050_ACCEL_NORM_MAX_G)))
	{
		return 0U;
	}

	if ((BOARD_YAW_MAG_TRUST_WHEN_STATIC_ONLY != 0U) &&
		(fabsf(MPU6050_GetGyroZDps()) > BOARD_YAW_STATIC_GYRO_DPS_THRESHOLD))
	{
		return 0U;
	}

	return 1U;
}

static uint8_t IMU_Fusion_IsRelockStable(void)
{
	if (MPU6050_IsAccelTiltValid() == 0U)
	{
		return 0U;
	}

	if ((fabsf(MPU6050_GetPitchTiltDeg()) > BOARD_YAW_MAG_RECOVER_MAX_TILT_DEG) ||
		(fabsf(MPU6050_GetRollTiltDeg()) > BOARD_YAW_MAG_RECOVER_MAX_TILT_DEG))
	{
		return 0U;
	}

	if (fabsf(MPU6050_GetGyroZDps()) > BOARD_YAW_MAG_STABLE_GYRO_DPS_THRESHOLD)
	{
		return 0U;
	}

	return 1U;
}

static uint8_t IMU_Fusion_ShouldHoldRecovery(void)
{
	if (MPU6050_IsAccelTiltValid() == 0U)
	{
		return 1U;
	}

	if ((fabsf(MPU6050_GetPitchTiltDeg()) > BOARD_YAW_MAG_RECOVER_MAX_TILT_DEG) ||
		(fabsf(MPU6050_GetRollTiltDeg()) > BOARD_YAW_MAG_RECOVER_MAX_TILT_DEG))
	{
		return 1U;
	}

	return 0U;
}

static uint8_t IMU_Fusion_PrepareMagHeading(float *preferredHeadingDeg, uint8_t allowRawFallback)
{
	float rawHeadingDeg;
	float pitchDeg;
	float rollDeg;
	float accNormG;

	if (preferredHeadingDeg == 0)
	{
		return 0U;
	}

	g_magCorrectionValid = 0U;
	if ((HMC5883L_IsOnline() == 0U) || (HMC5883L_IsHeadingValid() == 0U))
	{
		return 0U;
	}

	rawHeadingDeg = Angle_Wrap180(HMC5883L_GetHeadingDeg());
	g_magHeadingRawDeg = rawHeadingDeg;

#if ((BOARD_YAW_TILT_COMPENSATION_ENABLE != 0U) && (BOARD_YAW_MAG_HEADING_USE_TILT_COMP != 0U))
	pitchDeg = MPU6050_GetPitchTiltDeg();
	rollDeg = MPU6050_GetRollTiltDeg();
	accNormG = MPU6050_GetAccelNormG();

	if (IMU_Fusion_IsTiltCompensationUsable(pitchDeg, rollDeg, accNormG) != 0U)
	{
		g_magHeadingTiltCompDeg = IMU_Fusion_ComputeTiltCompensatedHeadingDeg(HMC5883L_GetFieldX(),
																		  HMC5883L_GetFieldY(),
																		  HMC5883L_GetFieldZ(),
																		  pitchDeg,
																		  rollDeg);
		*preferredHeadingDeg = g_magHeadingTiltCompDeg;
		g_magCorrectionValid = 1U;
		return 1U;
	}

	if (allowRawFallback == 0U)
	{
		return 0U;
	}

	g_magHeadingTiltCompDeg = rawHeadingDeg;
	*preferredHeadingDeg = rawHeadingDeg;
	return 1U;
#else
	g_magHeadingTiltCompDeg = rawHeadingDeg;
	*preferredHeadingDeg = rawHeadingDeg;
	g_magCorrectionValid = 1U;
	return 1U;
#endif
}

void IMU_Fusion_Init(void)
{
	float initialYawDeg;

	g_magHeadingDeg = 0.0f;
	g_magHeadingRawDeg = 0.0f;
	g_magHeadingTiltCompDeg = 0.0f;
	g_magHeadingReady = 0U;
	g_magCorrectionValid = 0U;
	g_magRecoveryStableMs = 0U;
	g_magRecoveryPending = 0U;
	g_magRecoveryBoostActive = 0U;

	initialYawDeg = 0.0f;
	if (IMU_Fusion_PrepareMagHeading(&initialYawDeg, 1U) != 0U)
	{
		g_magHeadingDeg = Angle_Wrap180(initialYawDeg);
		g_magHeadingReady = 1U;
		if (g_magCorrectionValid == 0U)
		{
			g_magRecoveryPending = 1U;
		}
	}

	g_fusedYawDeg = Angle_Wrap180(initialYawDeg);
	g_gyroHeadingDeg = g_fusedYawDeg;
	MPU6050_SetYawGyroOnlyDeg(g_fusedYawDeg);
	MPU6050_SetFusedYawDeg(g_fusedYawDeg);
}

void IMU_Fusion_SampleTask(void)
{
	float currentGyroHeadingDeg;
	float predictedYawDeg;
	float magReferenceDeg;
	float correctionDeg;
	float correctionGain;
	uint32_t nextStableMs;

	currentGyroHeadingDeg = MPU6050_GetYawGyroOnlyDeg();
	predictedYawDeg = Angle_Wrap180(g_fusedYawDeg +
									Angle_DiffDeg(currentGyroHeadingDeg, g_gyroHeadingDeg));
	g_gyroHeadingDeg = currentGyroHeadingDeg;

	if (IMU_Fusion_ShouldHoldRecovery() != 0U)
	{
		g_magRecoveryPending = 1U;
		g_magRecoveryStableMs = 0U;
		g_magRecoveryBoostActive = 0U;
	}

	if (IMU_Fusion_PrepareMagHeading(&magReferenceDeg, 0U) == 0U)
	{
		g_magRecoveryPending = 1U;
		g_magRecoveryStableMs = 0U;
		g_magRecoveryBoostActive = 0U;
		g_fusedYawDeg = predictedYawDeg;
		MPU6050_SetFusedYawDeg(g_fusedYawDeg);
		return;
	}

	if (g_magRecoveryPending != 0U)
	{
		if (IMU_Fusion_IsRelockStable() != 0U)
		{
			nextStableMs = (uint32_t)g_magRecoveryStableMs + BOARD_MPU6050_SAMPLE_MS;
			if (nextStableMs > BOARD_YAW_MAG_RECOVER_STABLE_MS)
			{
				nextStableMs = BOARD_YAW_MAG_RECOVER_STABLE_MS;
			}

			g_magRecoveryStableMs = (uint16_t)nextStableMs;
			if (g_magRecoveryStableMs >= BOARD_YAW_MAG_RECOVER_STABLE_MS)
			{
				g_magHeadingDeg = Angle_Wrap180(magReferenceDeg);
				g_magHeadingReady = 1U;
				g_magRecoveryPending = 0U;
				g_magRecoveryBoostActive = 1U;
			}
		}
		else
		{
			g_magRecoveryStableMs = 0U;
		}
	}

	if (g_magHeadingReady == 0U)
	{
		g_magHeadingDeg = Angle_Wrap180(magReferenceDeg);
		g_magHeadingReady = 1U;
	}

	if ((g_magCorrectionValid != 0U) && (g_magRecoveryPending == 0U))
	{
		g_magHeadingDeg = Angle_Wrap180(g_magHeadingDeg +
										(BOARD_YAW_MAG_LPF_ALPHA *
										 Angle_DiffDeg(magReferenceDeg, g_magHeadingDeg)));

		if (BOARD_YAW_FUSION_ENABLE != 0U)
		{
			correctionDeg = Angle_DiffDeg(g_magHeadingDeg, predictedYawDeg);
			correctionGain = (1.0f - BOARD_YAW_COMPLEMENTARY_ALPHA);

			if ((g_magRecoveryBoostActive != 0U) &&
				(fabsf(correctionDeg) > BOARD_YAW_MAG_RECOVER_EXIT_ERROR_DEG) &&
				(BOARD_YAW_MAG_RECOVER_GAIN > correctionGain))
			{
				correctionGain = BOARD_YAW_MAG_RECOVER_GAIN;
			}
			else if (fabsf(correctionDeg) <= BOARD_YAW_MAG_RECOVER_EXIT_ERROR_DEG)
			{
				g_magRecoveryBoostActive = 0U;
			}

			g_fusedYawDeg = Angle_Wrap180(predictedYawDeg + (correctionGain * correctionDeg));
		}
		else
		{
			g_fusedYawDeg = predictedYawDeg;
			g_magRecoveryBoostActive = 0U;
		}
	}
	else
	{
		g_fusedYawDeg = predictedYawDeg;
	}

	MPU6050_SetFusedYawDeg(g_fusedYawDeg);
}

float IMU_Fusion_GetYawDeg(void)
{
	return g_fusedYawDeg;
}

float IMU_Fusion_GetMagHeadingDeg(void)
{
	return g_magHeadingDeg;
}

float IMU_Fusion_GetMagHeadingRawDeg(void)
{
	return g_magHeadingRawDeg;
}

float IMU_Fusion_GetMagHeadingTiltCompDeg(void)
{
	return g_magHeadingTiltCompDeg;
}

float IMU_Fusion_GetGyroHeadingDeg(void)
{
	return g_gyroHeadingDeg;
}

uint8_t IMU_Fusion_IsMagCorrectionValid(void)
{
	return g_magCorrectionValid;
}

uint8_t IMU_Fusion_IsMagRecoveryPending(void)
{
	return g_magRecoveryPending;
}

void IMU_Fusion_ResetYaw(float yawDeg)
{
	float magHeadingDeg;

	g_fusedYawDeg = Angle_Wrap180(yawDeg);
	g_gyroHeadingDeg = g_fusedYawDeg;
	MPU6050_SetYawGyroOnlyDeg(g_fusedYawDeg);
	MPU6050_SetFusedYawDeg(g_fusedYawDeg);
	g_magRecoveryStableMs = 0U;
	g_magRecoveryPending = 0U;
	g_magRecoveryBoostActive = 0U;

	if (IMU_Fusion_PrepareMagHeading(&magHeadingDeg, 1U) != 0U)
	{
		g_magHeadingDeg = Angle_Wrap180(magHeadingDeg);
		g_magHeadingReady = 1U;
		if (g_magCorrectionValid == 0U)
		{
			g_magRecoveryPending = 1U;
		}
	}
	else
	{
		g_magHeadingReady = 0U;
		g_magCorrectionValid = 0U;
		g_magRecoveryPending = 1U;
	}
}
