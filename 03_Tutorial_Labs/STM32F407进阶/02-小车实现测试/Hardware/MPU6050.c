#include "MPU6050.h"
#include "BoardConfig.h"
#include "BoardI2C.h"
#include "Delay.h"
#include "HMC5883L.h"
#include "IMU_Fusion.h"
#include <math.h>

#define MPU6050_REG_SMPLRT_DIV                 0x19U
#define MPU6050_REG_CONFIG                     0x1AU
#define MPU6050_REG_GYRO_CONFIG                0x1BU
#define MPU6050_REG_ACCEL_CONFIG               0x1CU
#define MPU6050_REG_ACCEL_XOUT_H               0x3BU
#define MPU6050_REG_ACCEL_XOUT_L               0x3CU
#define MPU6050_REG_ACCEL_YOUT_H               0x3DU
#define MPU6050_REG_ACCEL_YOUT_L               0x3EU
#define MPU6050_REG_ACCEL_ZOUT_H               0x3FU
#define MPU6050_REG_ACCEL_ZOUT_L               0x40U
#define MPU6050_REG_GYRO_XOUT_H                0x43U
#define MPU6050_REG_GYRO_XOUT_L                0x44U
#define MPU6050_REG_GYRO_YOUT_H                0x45U
#define MPU6050_REG_GYRO_YOUT_L                0x46U
#define MPU6050_REG_GYRO_ZOUT_H                0x47U
#define MPU6050_REG_GYRO_ZOUT_L                0x48U
#define MPU6050_REG_PWR_MGMT_1                 0x6BU
#define MPU6050_REG_WHO_AM_I                   0x75U

#define MPU6050_DEG_TO_RAD                     0.01745329252f
#define MPU6050_RAD_TO_DEG                     57.2957795f

static volatile uint8_t g_mpuOnline = 0U;
static volatile uint8_t g_mpuCalibrated = 0U;

static volatile int16_t g_rawGyroX = 0;
static volatile int16_t g_rawGyroY = 0;
static volatile int16_t g_rawGyroZ = 0;
static volatile int16_t g_rawAccelX = 0;
static volatile int16_t g_rawAccelY = 0;
static volatile int16_t g_rawAccelZ = 0;

static volatile float g_gyroXDps = 0.0f;
static volatile float g_gyroYDps = 0.0f;
static volatile float g_gyroBiasZDps = 0.0f;
static volatile float g_gyroZDps = 0.0f;
static volatile float g_accelXG = 0.0f;
static volatile float g_accelYG = 0.0f;
static volatile float g_accelZG = 0.0f;
static volatile float g_accNormG = 0.0f;
static volatile float g_pitchAccDeg = 0.0f;
static volatile float g_rollAccDeg = 0.0f;
static volatile float g_pitchTiltDeg = 0.0f;
static volatile float g_rollTiltDeg = 0.0f;
static volatile float g_yawGyroOnlyDeg = 0.0f;
static volatile float g_fusedYawDeg = 0.0f;

static uint8_t g_accelTiltReady = 0U;
static volatile uint8_t g_accelTiltValid = 0U;

static float MPU6050_WrapAngle180(float angleDeg)
{
	while (angleDeg > 180.0f)
	{
		angleDeg -= 360.0f;
	}

	while (angleDeg < -180.0f)
	{
		angleDeg += 360.0f;
	}

	return angleDeg;
}

static float MPU6050_AngleDiffDeg(float targetDeg, float currentDeg)
{
	return MPU6050_WrapAngle180(targetDeg - currentDeg);
}

static void MPU6050_ComputeEulerRateDeg(float rollDeg,
										float pitchDeg,
										float gyroXDps,
										float gyroYDps,
										float gyroZDps,
										float *rollRateDeg,
										float *pitchRateDeg,
										float *yawRateDeg)
{
	float rollRad;
	float pitchRad;
	float gxRad;
	float gyRad;
	float gzRad;
	float sinRoll;
	float cosRoll;
	float sinPitch;
	float cosPitch;
	float invCosPitch;
	float tanPitch;

	rollRad = rollDeg * MPU6050_DEG_TO_RAD;
	pitchRad = pitchDeg * MPU6050_DEG_TO_RAD;
	gxRad = gyroXDps * MPU6050_DEG_TO_RAD;
	gyRad = gyroYDps * MPU6050_DEG_TO_RAD;
	gzRad = gyroZDps * MPU6050_DEG_TO_RAD;

	sinRoll = sinf(rollRad);
	cosRoll = cosf(rollRad);
	sinPitch = sinf(pitchRad);
	cosPitch = cosf(pitchRad);
	if (fabsf(cosPitch) < 0.17364818f)
	{
		cosPitch = (cosPitch >= 0.0f) ? 0.17364818f : -0.17364818f;
	}

	invCosPitch = 1.0f / cosPitch;
	tanPitch = sinPitch * invCosPitch;

	if (rollRateDeg != 0)
	{
		*rollRateDeg = (gxRad + (sinRoll * tanPitch * gyRad) + (cosRoll * tanPitch * gzRad)) * MPU6050_RAD_TO_DEG;
	}

	if (pitchRateDeg != 0)
	{
		*pitchRateDeg = ((cosRoll * gyRad) - (sinRoll * gzRad)) * MPU6050_RAD_TO_DEG;
	}

	if (yawRateDeg != 0)
	{
		*yawRateDeg = ((sinRoll * invCosPitch * gyRad) + (cosRoll * invCosPitch * gzRad)) * MPU6050_RAD_TO_DEG;
	}
}

static void MPU6050_ApplyAccelAxisOrientation(int16_t *accelX, int16_t *accelY, int16_t *accelZ)
{
	int16_t temp;

	if ((accelX == 0) || (accelY == 0) || (accelZ == 0))
	{
		return;
	}

	if (BOARD_MPU6050_ACCEL_XY_SWAP != 0U)
	{
		temp = *accelX;
		*accelX = *accelY;
		*accelY = temp;
	}

	if (BOARD_MPU6050_ACCEL_X_REVERSE != 0U)
	{
		*accelX = (int16_t)(-*accelX);
	}

	if (BOARD_MPU6050_ACCEL_Y_REVERSE != 0U)
	{
		*accelY = (int16_t)(-*accelY);
	}

	if (BOARD_MPU6050_ACCEL_Z_REVERSE != 0U)
	{
		*accelZ = (int16_t)(-*accelZ);
	}
}

static void MPU6050_ApplyGyroAxisOrientation(int16_t *gyroX, int16_t *gyroY, int16_t *gyroZ)
{
	int16_t temp;

	if ((gyroX == 0) || (gyroY == 0) || (gyroZ == 0))
	{
		return;
	}

	if (BOARD_MPU6050_GYRO_XY_SWAP != 0U)
	{
		temp = *gyroX;
		*gyroX = *gyroY;
		*gyroY = temp;
	}

	if (BOARD_MPU6050_GYRO_X_REVERSE != 0U)
	{
		*gyroX = (int16_t)(-*gyroX);
	}

	if (BOARD_MPU6050_GYRO_Y_REVERSE != 0U)
	{
		*gyroY = (int16_t)(-*gyroY);
	}

	if (BOARD_MPU6050_GYRO_Z_REVERSE != 0U)
	{
		*gyroZ = (int16_t)(-*gyroZ);
	}
}

static ErrorStatus MPU6050_ReadRawAccelXYZ(int16_t *accelX, int16_t *accelY, int16_t *accelZ)
{
	uint8_t dataBuffer[6];
	int16_t localAccelX;
	int16_t localAccelY;
	int16_t localAccelZ;

	if ((accelX == 0) || (accelY == 0) || (accelZ == 0))
	{
		return ERROR;
	}

	if (BoardI2C1_ReadBytes(BOARD_MPU6050_I2C_ADDRESS,
							 MPU6050_REG_ACCEL_XOUT_H,
							 dataBuffer,
							 6U) != SUCCESS)
	{
		g_mpuOnline = 0U;
		return ERROR;
	}

	localAccelX = (int16_t)((dataBuffer[0] << 8) | dataBuffer[1]);
	localAccelY = (int16_t)((dataBuffer[2] << 8) | dataBuffer[3]);
	localAccelZ = (int16_t)((dataBuffer[4] << 8) | dataBuffer[5]);
	MPU6050_ApplyAccelAxisOrientation(&localAccelX, &localAccelY, &localAccelZ);

	*accelX = localAccelX;
	*accelY = localAccelY;
	*accelZ = localAccelZ;
	return SUCCESS;
}

static ErrorStatus MPU6050_ReadRawGyroXYZ(int16_t *gyroX, int16_t *gyroY, int16_t *gyroZ)
{
	uint8_t dataBuffer[6];
	int16_t localGyroX;
	int16_t localGyroY;
	int16_t localGyroZ;

	if ((gyroX == 0) || (gyroY == 0) || (gyroZ == 0))
	{
		return ERROR;
	}

	if (BoardI2C1_ReadBytes(BOARD_MPU6050_I2C_ADDRESS,
							 MPU6050_REG_GYRO_XOUT_H,
							 dataBuffer,
							 6U) != SUCCESS)
	{
		g_mpuOnline = 0U;
		return ERROR;
	}

	localGyroX = (int16_t)((dataBuffer[0] << 8) | dataBuffer[1]);
	localGyroY = (int16_t)((dataBuffer[2] << 8) | dataBuffer[3]);
	localGyroZ = (int16_t)((dataBuffer[4] << 8) | dataBuffer[5]);
	MPU6050_ApplyGyroAxisOrientation(&localGyroX, &localGyroY, &localGyroZ);

	*gyroX = localGyroX;
	*gyroY = localGyroY;
	*gyroZ = localGyroZ;
	return SUCCESS;
}

static int16_t MPU6050_ReadRawGyroZ(void)
{
	uint8_t dataBuffer[2];
	int16_t rawGyroZ;

	if (BoardI2C1_ReadBytes(BOARD_MPU6050_I2C_ADDRESS,
							 MPU6050_REG_GYRO_ZOUT_H,
							 dataBuffer,
							 2U) != SUCCESS)
	{
		g_mpuOnline = 0U;
		return g_rawGyroZ;
	}

	rawGyroZ = (int16_t)((dataBuffer[0] << 8) | dataBuffer[1]);
	if (BOARD_MPU6050_GYRO_Z_REVERSE != 0U)
	{
		rawGyroZ = (int16_t)(-rawGyroZ);
	}

	return rawGyroZ;
}

static float MPU6050_ConvertRawGyroToDps(int16_t rawGyro)
{
	return ((float)rawGyro / BOARD_MPU6050_GYRO_SENSITIVITY_LSB);
}

static float MPU6050_ConvertRawAccelToG(int16_t rawAccel)
{
	return ((float)rawAccel / BOARD_MPU6050_ACCEL_SENSITIVITY_LSB);
}

static void MPU6050_UpdateTiltFromAccel(void)
{
	float pitchAccDeg;
	float rollAccDeg;
	float pitchDenominator;
	float predictedPitchDeg;
	float predictedRollDeg;
	float pitchRateDeg;
	float rollRateDeg;
	float tiltRateDps;

	g_accNormG = sqrtf((g_accelXG * g_accelXG) +
					  (g_accelYG * g_accelYG) +
					  (g_accelZG * g_accelZG));

	pitchDenominator = sqrtf((g_accelYG * g_accelYG) + (g_accelZG * g_accelZG));
	if (pitchDenominator < 0.0001f)
	{
		pitchDenominator = 0.0001f;
	}

	/* 使用加速度计估计重力方向，在小动态场景下得到 pitch / roll。 */
	rollAccDeg = atan2f(g_accelYG, g_accelZG) * MPU6050_RAD_TO_DEG;
	pitchAccDeg = atan2f(-g_accelXG, pitchDenominator) * MPU6050_RAD_TO_DEG;
	g_pitchAccDeg = pitchAccDeg;
	g_rollAccDeg = rollAccDeg;

	if (g_accelTiltReady == 0U)
	{
		if ((g_accNormG < BOARD_MPU6050_ACCEL_NORM_MIN_G) ||
			(g_accNormG > BOARD_MPU6050_ACCEL_NORM_MAX_G))
		{
			g_accelTiltValid = 0U;
			return;
		}

		g_pitchTiltDeg = pitchAccDeg;
		g_rollTiltDeg = rollAccDeg;
		g_accelTiltReady = 1U;
		g_accelTiltValid = 1U;
		return;
	}

	MPU6050_ComputeEulerRateDeg(g_rollTiltDeg,
								g_pitchTiltDeg,
								g_gyroXDps,
								g_gyroYDps,
								g_gyroZDps,
								&rollRateDeg,
								&pitchRateDeg,
								0);
	predictedPitchDeg = MPU6050_WrapAngle180(g_pitchTiltDeg +
											(pitchRateDeg * ((float)BOARD_MPU6050_SAMPLE_MS / 1000.0f)));
	predictedRollDeg = MPU6050_WrapAngle180(g_rollTiltDeg +
										   (rollRateDeg * ((float)BOARD_MPU6050_SAMPLE_MS / 1000.0f)));
	g_pitchTiltDeg = predictedPitchDeg;
	g_rollTiltDeg = predictedRollDeg;

	tiltRateDps = sqrtf((g_gyroXDps * g_gyroXDps) + (g_gyroYDps * g_gyroYDps));
	if ((g_accNormG < BOARD_MPU6050_ACCEL_NORM_MIN_G) ||
		(g_accNormG > BOARD_MPU6050_ACCEL_NORM_MAX_G) ||
		(tiltRateDps > BOARD_MPU6050_TILT_CORRECTION_GYRO_DPS_THRESHOLD))
	{
		g_accelTiltValid = 0U;
		return;
	}

	/* X/Y 轴转动平缓时，再用加速度把倾角慢慢拉回重力方向。 */
	g_pitchTiltDeg = MPU6050_WrapAngle180(predictedPitchDeg +
										 (BOARD_MPU6050_TILT_CORRECTION_ALPHA *
										  MPU6050_AngleDiffDeg(pitchAccDeg, predictedPitchDeg)));
	g_rollTiltDeg = MPU6050_WrapAngle180(predictedRollDeg +
										(BOARD_MPU6050_TILT_CORRECTION_ALPHA *
										 MPU6050_AngleDiffDeg(rollAccDeg, predictedRollDeg)));
	g_accelTiltValid = 1U;
}

static void MPU6050_UpdateDynamicBias(float rawGyroZDps)
{
	float unbiasedGyroDps;

	if (BOARD_YAW_GYRO_REBIAS_ENABLE == 0U)
	{
		return;
	}

	if ((g_accNormG < BOARD_MPU6050_ACCEL_NORM_MIN_G) ||
		(g_accNormG > BOARD_MPU6050_ACCEL_NORM_MAX_G))
	{
		return;
	}

	if (g_accelTiltValid == 0U)
	{
		return;
	}

	unbiasedGyroDps = rawGyroZDps - g_gyroBiasZDps;
	if (fabsf(unbiasedGyroDps) > BOARD_YAW_STATIC_GYRO_DPS_THRESHOLD)
	{
		return;
	}

	g_gyroBiasZDps = (BOARD_YAW_GYRO_REBIAS_ALPHA * rawGyroZDps) +
					 ((1.0f - BOARD_YAW_GYRO_REBIAS_ALPHA) * g_gyroBiasZDps);
}

void MPU6050_StartGyroBiasCalibration(void)
{
	uint16_t sampleIndex;
	uint16_t validSamples;
	float biasSum;
	int16_t rawGyroZ;
	float alignYawDeg;

	if (g_mpuOnline == 0U)
	{
		g_mpuCalibrated = 0U;
		g_gyroBiasZDps = 0.0f;
		return;
	}

	biasSum = 0.0f;
	validSamples = 0U;
	for (sampleIndex = 0U; sampleIndex < BOARD_MPU6050_CALIBRATION_SAMPLES; sampleIndex++)
	{
		rawGyroZ = MPU6050_ReadRawGyroZ();
		if (g_mpuOnline == 0U)
		{
			break;
		}

		biasSum += MPU6050_ConvertRawGyroToDps(rawGyroZ);
		validSamples++;
		Delay_ms(BOARD_MPU6050_CALIBRATION_DELAY_MS);
	}

	if (validSamples == 0U)
	{
		g_mpuCalibrated = 0U;
		g_gyroBiasZDps = 0.0f;
		return;
	}

	g_gyroBiasZDps = biasSum / (float)validSamples;
	g_gyroXDps = 0.0f;
	g_gyroYDps = 0.0f;
	g_gyroZDps = 0.0f;

	/* 零偏重标后，将陀螺积分航向重新对齐到当前磁参考。 */
	alignYawDeg = g_fusedYawDeg;
	if ((HMC5883L_IsOnline() != 0U) && (HMC5883L_IsHeadingValid() != 0U))
	{
		alignYawDeg = HMC5883L_GetHeadingDeg();
	}

	g_yawGyroOnlyDeg = MPU6050_WrapAngle180(alignYawDeg);
	g_fusedYawDeg = g_yawGyroOnlyDeg;
	g_mpuCalibrated = 1U;
	IMU_Fusion_ResetYaw(alignYawDeg);
}

void MPU6050_Init(void)
{
	uint8_t whoAmI;
	int16_t rawAccelX;
	int16_t rawAccelY;
	int16_t rawAccelZ;

	g_mpuOnline = 0U;
	g_mpuCalibrated = 0U;
	g_rawGyroX = 0;
	g_rawGyroY = 0;
	g_rawGyroZ = 0;
	g_rawAccelX = 0;
	g_rawAccelY = 0;
	g_rawAccelZ = 0;
	g_gyroXDps = 0.0f;
	g_gyroYDps = 0.0f;
	g_gyroBiasZDps = 0.0f;
	g_gyroZDps = 0.0f;
	g_accelXG = 0.0f;
	g_accelYG = 0.0f;
	g_accelZG = 0.0f;
	g_accNormG = 0.0f;
	g_pitchAccDeg = 0.0f;
	g_rollAccDeg = 0.0f;
	g_pitchTiltDeg = 0.0f;
	g_rollTiltDeg = 0.0f;
	g_yawGyroOnlyDeg = 0.0f;
	g_fusedYawDeg = 0.0f;
	g_accelTiltReady = 0U;
	g_accelTiltValid = 0U;

	BoardI2C1_InitOnce();
	HMC5883L_Init();
	Delay_ms(20U);

	if (BoardI2C1_ReadByte(BOARD_MPU6050_I2C_ADDRESS, MPU6050_REG_WHO_AM_I, &whoAmI) != SUCCESS)
	{
		IMU_Fusion_Init();
		return;
	}

	if (whoAmI != BOARD_MPU6050_WHO_AM_I_EXPECTED)
	{
		IMU_Fusion_Init();
		return;
	}

	g_mpuOnline = 1U;

	if (BoardI2C1_WriteByte(BOARD_MPU6050_I2C_ADDRESS, MPU6050_REG_PWR_MGMT_1, 0x00U) != SUCCESS)
	{
		g_mpuOnline = 0U;
		IMU_Fusion_Init();
		return;
	}

	(void)BoardI2C1_WriteByte(BOARD_MPU6050_I2C_ADDRESS, MPU6050_REG_SMPLRT_DIV, 0x07U);
	(void)BoardI2C1_WriteByte(BOARD_MPU6050_I2C_ADDRESS, MPU6050_REG_CONFIG, 0x03U);
	(void)BoardI2C1_WriteByte(BOARD_MPU6050_I2C_ADDRESS, MPU6050_REG_GYRO_CONFIG, 0x00U);
	(void)BoardI2C1_WriteByte(BOARD_MPU6050_I2C_ADDRESS,
							 MPU6050_REG_ACCEL_CONFIG,
							 BOARD_MPU6050_ACCEL_CONFIG_VALUE);

	Delay_ms(50U);

	if (MPU6050_ReadRawAccelXYZ(&rawAccelX, &rawAccelY, &rawAccelZ) == SUCCESS)
	{
		g_rawAccelX = rawAccelX;
		g_rawAccelY = rawAccelY;
		g_rawAccelZ = rawAccelZ;
		g_accelXG = MPU6050_ConvertRawAccelToG(g_rawAccelX);
		g_accelYG = MPU6050_ConvertRawAccelToG(g_rawAccelY);
		g_accelZG = MPU6050_ConvertRawAccelToG(g_rawAccelZ);
		MPU6050_UpdateTiltFromAccel();
	}

	HMC5883L_SampleTask();
	MPU6050_StartGyroBiasCalibration();
	IMU_Fusion_Init();
}

void MPU6050_SampleTask(void)
{
	int16_t rawAccelX;
	int16_t rawAccelY;
	int16_t rawAccelZ;
	int16_t rawGyroX;
	int16_t rawGyroY;
	int16_t rawGyroZ;
	float rawGyroZDps;
	float newGyroZDps;
	float yawRateDeg;

	if (g_mpuOnline != 0U)
	{
		if (MPU6050_ReadRawAccelXYZ(&rawAccelX, &rawAccelY, &rawAccelZ) != SUCCESS)
		{
			HMC5883L_SampleTask();
			IMU_Fusion_SampleTask();
			return;
		}

		g_rawAccelX = rawAccelX;
		g_rawAccelY = rawAccelY;
		g_rawAccelZ = rawAccelZ;
		g_accelXG = MPU6050_ConvertRawAccelToG(g_rawAccelX);
		g_accelYG = MPU6050_ConvertRawAccelToG(g_rawAccelY);
		g_accelZG = MPU6050_ConvertRawAccelToG(g_rawAccelZ);

		if (MPU6050_ReadRawGyroXYZ(&rawGyroX, &rawGyroY, &rawGyroZ) != SUCCESS)
		{
			HMC5883L_SampleTask();
			IMU_Fusion_SampleTask();
			return;
		}

		g_rawGyroX = rawGyroX;
		g_rawGyroY = rawGyroY;
		g_rawGyroZ = rawGyroZ;
		g_gyroXDps = MPU6050_ConvertRawGyroToDps(g_rawGyroX);
		g_gyroYDps = MPU6050_ConvertRawGyroToDps(g_rawGyroY);
		rawGyroZDps = MPU6050_ConvertRawGyroToDps(g_rawGyroZ);
		MPU6050_UpdateDynamicBias(rawGyroZDps);
		newGyroZDps = rawGyroZDps - g_gyroBiasZDps;
		g_gyroZDps = (BOARD_MPU6050_GYRO_LPF_ALPHA * newGyroZDps) +
					((1.0f - BOARD_MPU6050_GYRO_LPF_ALPHA) * g_gyroZDps);
		MPU6050_UpdateTiltFromAccel();

		MPU6050_ComputeEulerRateDeg(g_rollTiltDeg,
									g_pitchTiltDeg,
									g_gyroXDps,
									g_gyroYDps,
									g_gyroZDps,
									0,
									0,
									&yawRateDeg);

		g_yawGyroOnlyDeg += yawRateDeg * ((float)BOARD_MPU6050_SAMPLE_MS / 1000.0f);
		g_yawGyroOnlyDeg = MPU6050_WrapAngle180(g_yawGyroOnlyDeg);
	}

	HMC5883L_SampleTask();
	IMU_Fusion_SampleTask();
}

void MPU6050_ResetYaw(void)
{
	g_yawGyroOnlyDeg = 0.0f;
	g_fusedYawDeg = 0.0f;
	IMU_Fusion_ResetYaw(0.0f);
}

uint8_t MPU6050_IsOnline(void)
{
	return g_mpuOnline;
}

uint8_t MPU6050_IsCalibrated(void)
{
	return g_mpuCalibrated;
}

int16_t MPU6050_GetRawGyroZ(void)
{
	return g_rawGyroZ;
}

int16_t MPU6050_GetRawGyroX(void)
{
	return g_rawGyroX;
}

int16_t MPU6050_GetRawGyroY(void)
{
	return g_rawGyroY;
}

int16_t MPU6050_GetRawAccelX(void)
{
	return g_rawAccelX;
}

int16_t MPU6050_GetRawAccelY(void)
{
	return g_rawAccelY;
}

int16_t MPU6050_GetRawAccelZ(void)
{
	return g_rawAccelZ;
}

float MPU6050_GetGyroZDps(void)
{
	return g_gyroZDps;
}

float MPU6050_GetGyroXDps(void)
{
	return g_gyroXDps;
}

float MPU6050_GetGyroYDps(void)
{
	return g_gyroYDps;
}

float MPU6050_GetAccelXG(void)
{
	return g_accelXG;
}

float MPU6050_GetAccelYG(void)
{
	return g_accelYG;
}

float MPU6050_GetAccelZG(void)
{
	return g_accelZG;
}

float MPU6050_GetPitchAccDeg(void)
{
	return g_pitchAccDeg;
}

float MPU6050_GetRollAccDeg(void)
{
	return g_rollAccDeg;
}

float MPU6050_GetPitchTiltDeg(void)
{
	return g_pitchTiltDeg;
}

float MPU6050_GetRollTiltDeg(void)
{
	return g_rollTiltDeg;
}

float MPU6050_GetAccelNormG(void)
{
	return g_accNormG;
}

uint8_t MPU6050_IsAccelTiltValid(void)
{
	return g_accelTiltValid;
}

float MPU6050_GetYawDeg(void)
{
	return g_fusedYawDeg;
}

float MPU6050_GetYawGyroOnlyDeg(void)
{
	return g_yawGyroOnlyDeg;
}

float MPU6050_GetGyroBiasZDps(void)
{
	return g_gyroBiasZDps;
}

void MPU6050_SetYawGyroOnlyDeg(float yawDeg)
{
	g_yawGyroOnlyDeg = MPU6050_WrapAngle180(yawDeg);
}

void MPU6050_SetFusedYawDeg(float yawDeg)
{
	g_fusedYawDeg = MPU6050_WrapAngle180(yawDeg);
}
