#include "HMC5883L.h"
#include "BoardConfig.h"
#include "BoardI2C.h"
#include "Timer.h"
#include <math.h>

#define HMC5883L_REG_CONFIG_A                   0x00U
#define HMC5883L_REG_CONFIG_B                   0x01U
#define HMC5883L_REG_MODE                       0x02U
#define HMC5883L_REG_DATA_X_MSB                 0x03U
#define HMC5883L_REG_STATUS                     0x09U
#define HMC5883L_REG_ID_A                       0x0AU

#define HMC5883L_STATUS_LOCK                    0x02U
#define HMC5883L_STATUS_READY                   0x01U

#define HMC5883L_ID_A_EXPECTED                  ((uint8_t)'H')
#define HMC5883L_ID_B_EXPECTED                  ((uint8_t)'4')
#define HMC5883L_ID_C_EXPECTED                  ((uint8_t)'3')

#define HMC5883L_RAW_OVERFLOW_VALUE             (-4096)
#define HMC5883L_RAD_TO_DEG                     57.2957795f

static volatile uint8_t g_hmcOnline = 0U;
static volatile uint8_t g_hmcCalibrated = 0U;
static volatile uint8_t g_hmcCalibrating = 0U;
static volatile uint8_t g_hmcHeadingValid = 0U;
static volatile int16_t g_rawX = 0;
static volatile int16_t g_rawY = 0;
static volatile int16_t g_rawZ = 0;
static volatile float g_fieldX = 0.0f;
static volatile float g_fieldY = 0.0f;
static volatile float g_fieldZ = 0.0f;
static volatile float g_headingDeg = 0.0f;

static float g_runtimeOffsetX = 0.0f;
static float g_runtimeOffsetY = 0.0f;
static float g_runtimeOffsetZ = 0.0f;
static float g_runtimeScaleX = 1.0f;
static float g_runtimeScaleY = 1.0f;
static float g_runtimeScaleZ = 1.0f;

static int16_t g_calibMinX = 32767;
static int16_t g_calibMinY = 32767;
static int16_t g_calibMinZ = 32767;
static int16_t g_calibMaxX = -32768;
static int16_t g_calibMaxY = -32768;
static int16_t g_calibMaxZ = -32768;
static uint32_t g_lastSampleTick = 0U;
static uint8_t g_firstSamplePending = 1U;

static float HMC5883L_Wrap180(float angle)
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

static uint8_t HMC5883L_HasBoardCalibration(void)
{
	if (fabsf(BOARD_HMC5883L_HARD_IRON_OFFSET_X) > 0.001f)
	{
		return 1U;
	}

	if (fabsf(BOARD_HMC5883L_HARD_IRON_OFFSET_Y) > 0.001f)
	{
		return 1U;
	}

	if (fabsf(BOARD_HMC5883L_HARD_IRON_OFFSET_Z) > 0.001f)
	{
		return 1U;
	}

	if (fabsf(BOARD_HMC5883L_SOFT_IRON_SCALE_X - 1.0f) > 0.001f)
	{
		return 1U;
	}

	if (fabsf(BOARD_HMC5883L_SOFT_IRON_SCALE_Y - 1.0f) > 0.001f)
	{
		return 1U;
	}

	if (fabsf(BOARD_HMC5883L_SOFT_IRON_SCALE_Z - 1.0f) > 0.001f)
	{
		return 1U;
	}

	return 0U;
}

static uint8_t HMC5883L_AverageSamplesToBits(uint8_t samples)
{
	switch (samples)
	{
		case 1U:
			return 0x00U;

		case 2U:
			return 0x20U;

		case 4U:
			return 0x40U;

		case 8U:
		default:
			return 0x60U;
	}
}

static uint8_t HMC5883L_OutputRateToBits(uint8_t rateHz)
{
	switch (rateHz)
	{
		case 1U:
			return 0x04U;

		case 3U:
			return 0x08U;

		case 7U:
			return 0x0CU;

		case 15U:
			return 0x10U;

		case 30U:
			return 0x14U;

		case 75U:
			return 0x18U;

		case 0U:
		default:
			return 0x10U;
	}
}

static void HMC5883L_ResetRuntimeCalibration(void)
{
	g_runtimeOffsetX = 0.0f;
	g_runtimeOffsetY = 0.0f;
	g_runtimeOffsetZ = 0.0f;
	g_runtimeScaleX = 1.0f;
	g_runtimeScaleY = 1.0f;
	g_runtimeScaleZ = 1.0f;
}

static void HMC5883L_ResetCalibrationBounds(void)
{
	g_calibMinX = 32767;
	g_calibMinY = 32767;
	g_calibMinZ = 32767;
	g_calibMaxX = -32768;
	g_calibMaxY = -32768;
	g_calibMaxZ = -32768;
}

static void HMC5883L_ApplyAxisOrientation(int16_t *x, int16_t *y, int16_t *z)
{
	int16_t temp;

	if ((x == 0) || (y == 0) || (z == 0))
	{
		return;
	}

	if (BOARD_HMC5883L_XY_SWAP != 0U)
	{
		temp = *x;
		*x = *y;
		*y = temp;
	}

	if (BOARD_HMC5883L_X_REVERSE != 0U)
	{
		*x = (int16_t)(-*x);
	}

	if (BOARD_HMC5883L_Y_REVERSE != 0U)
	{
		*y = (int16_t)(-*y);
	}

	if (BOARD_HMC5883L_Z_REVERSE != 0U)
	{
		*z = (int16_t)(-*z);
	}
}

static void HMC5883L_UpdateCalibrationBounds(int16_t rawX, int16_t rawY, int16_t rawZ)
{
	if (rawX < g_calibMinX)
	{
		g_calibMinX = rawX;
	}
	if (rawX > g_calibMaxX)
	{
		g_calibMaxX = rawX;
	}

	if (rawY < g_calibMinY)
	{
		g_calibMinY = rawY;
	}
	if (rawY > g_calibMaxY)
	{
		g_calibMaxY = rawY;
	}

	if (rawZ < g_calibMinZ)
	{
		g_calibMinZ = rawZ;
	}
	if (rawZ > g_calibMaxZ)
	{
		g_calibMaxZ = rawZ;
	}
}

static void HMC5883L_RecalculateRuntimeCalibration(void)
{
	float radiusX;
	float radiusY;
	float radiusZ;
	float averageRadius;

	radiusX = ((float)g_calibMaxX - (float)g_calibMinX) * 0.5f;
	radiusY = ((float)g_calibMaxY - (float)g_calibMinY) * 0.5f;
	radiusZ = ((float)g_calibMaxZ - (float)g_calibMinZ) * 0.5f;

	if ((radiusX < BOARD_HMC5883L_CALIBRATION_MIN_SPAN_RAW) ||
		(radiusY < BOARD_HMC5883L_CALIBRATION_MIN_SPAN_RAW) ||
		(radiusZ < BOARD_HMC5883L_CALIBRATION_MIN_SPAN_RAW))
	{
		return;
	}

	g_runtimeOffsetX = ((float)g_calibMaxX + (float)g_calibMinX) * 0.5f;
	g_runtimeOffsetY = ((float)g_calibMaxY + (float)g_calibMinY) * 0.5f;
	g_runtimeOffsetZ = ((float)g_calibMaxZ + (float)g_calibMinZ) * 0.5f;

	averageRadius = (radiusX + radiusY + radiusZ) / 3.0f;
	if (radiusX > 0.1f)
	{
		g_runtimeScaleX = averageRadius / radiusX;
	}
	if (radiusY > 0.1f)
	{
		g_runtimeScaleY = averageRadius / radiusY;
	}
	if (radiusZ > 0.1f)
	{
		g_runtimeScaleZ = averageRadius / radiusZ;
	}

	g_hmcCalibrated = 1U;
}

static uint8_t HMC5883L_ReadChipId(void)
{
	uint8_t idBuffer[3];

	if (BoardI2C1_ReadBytes(BOARD_HMC5883L_I2C_ADDRESS,
							 HMC5883L_REG_ID_A,
							 idBuffer,
							 3U) != SUCCESS)
	{
		return 0U;
	}

	if ((idBuffer[0] != HMC5883L_ID_A_EXPECTED) ||
		(idBuffer[1] != HMC5883L_ID_B_EXPECTED) ||
		(idBuffer[2] != HMC5883L_ID_C_EXPECTED))
	{
		return 0U;
	}

	return 1U;
}

static uint8_t HMC5883L_IsRawValid(int16_t rawX, int16_t rawY, int16_t rawZ)
{
	if ((rawX == HMC5883L_RAW_OVERFLOW_VALUE) ||
		(rawY == HMC5883L_RAW_OVERFLOW_VALUE) ||
		(rawZ == HMC5883L_RAW_OVERFLOW_VALUE))
	{
		return 0U;
	}

	return 1U;
}

void HMC5883L_Init(void)
{
	uint8_t configA;

	g_hmcOnline = 0U;
	g_hmcHeadingValid = 0U;
	g_rawX = 0;
	g_rawY = 0;
	g_rawZ = 0;
	g_fieldX = 0.0f;
	g_fieldY = 0.0f;
	g_fieldZ = 0.0f;
	g_headingDeg = 0.0f;
	g_lastSampleTick = 0U;
	g_firstSamplePending = 1U;
	g_hmcCalibrating = 0U;
	g_hmcCalibrated = HMC5883L_HasBoardCalibration();
	HMC5883L_ResetRuntimeCalibration();
	HMC5883L_ResetCalibrationBounds();

	BoardI2C1_InitOnce();
	if (HMC5883L_ReadChipId() == 0U)
	{
		return;
	}

	configA = HMC5883L_AverageSamplesToBits(BOARD_HMC5883L_AVERAGE_SAMPLES) |
			  HMC5883L_OutputRateToBits(BOARD_HMC5883L_OUTPUT_RATE_HZ);

	if (BoardI2C1_WriteByte(BOARD_HMC5883L_I2C_ADDRESS,
							 HMC5883L_REG_CONFIG_A,
							 configA) != SUCCESS)
	{
		return;
	}

	if (BoardI2C1_WriteByte(BOARD_HMC5883L_I2C_ADDRESS,
							 HMC5883L_REG_CONFIG_B,
							 (uint8_t)(BOARD_HMC5883L_GAIN_CONFIG & 0xE0U)) != SUCCESS)
	{
		return;
	}

	if (BoardI2C1_WriteByte(BOARD_HMC5883L_I2C_ADDRESS,
							 HMC5883L_REG_MODE,
							 (uint8_t)(BOARD_HMC5883L_MODE_VALUE & 0x03U)) != SUCCESS)
	{
		return;
	}

	g_hmcOnline = 1U;
}

void HMC5883L_SampleTask(void)
{
	uint32_t nowTick;
	uint8_t status;
	uint8_t dataBuffer[6];
	int16_t rawX;
	int16_t rawY;
	int16_t rawZ;
	float correctedX;
	float correctedY;
	float correctedZ;
	float horizontalNorm;

	if (g_hmcOnline == 0U)
	{
		return;
	}

	nowTick = Timer_GetTick();
	if ((g_firstSamplePending == 0U) &&
		((nowTick - g_lastSampleTick) < BOARD_HMC5883L_SAMPLE_MS))
	{
		return;
	}

	g_lastSampleTick = nowTick;
	g_firstSamplePending = 0U;

	if (BoardI2C1_ReadByte(BOARD_HMC5883L_I2C_ADDRESS,
							HMC5883L_REG_STATUS,
							&status) != SUCCESS)
	{
		g_hmcOnline = 0U;
		g_hmcHeadingValid = 0U;
		return;
	}

	if (((status & HMC5883L_STATUS_READY) == 0U) && ((status & HMC5883L_STATUS_LOCK) == 0U))
	{
		return;
	}

	if (BoardI2C1_ReadBytes(BOARD_HMC5883L_I2C_ADDRESS,
							 HMC5883L_REG_DATA_X_MSB,
							 dataBuffer,
							 6U) != SUCCESS)
	{
		g_hmcOnline = 0U;
		g_hmcHeadingValid = 0U;
		return;
	}

	/* HMC5883L 数据顺序固定为 X, Z, Y。 */
	rawX = (int16_t)((dataBuffer[0] << 8) | dataBuffer[1]);
	rawZ = (int16_t)((dataBuffer[2] << 8) | dataBuffer[3]);
	rawY = (int16_t)((dataBuffer[4] << 8) | dataBuffer[5]);

	HMC5883L_ApplyAxisOrientation(&rawX, &rawY, &rawZ);
	if (HMC5883L_IsRawValid(rawX, rawY, rawZ) == 0U)
	{
		g_hmcHeadingValid = 0U;
		return;
	}

	g_rawX = rawX;
	g_rawY = rawY;
	g_rawZ = rawZ;

	if (g_hmcCalibrating != 0U)
	{
		HMC5883L_UpdateCalibrationBounds(rawX, rawY, rawZ);
		HMC5883L_RecalculateRuntimeCalibration();
	}

	correctedX = ((float)rawX - BOARD_HMC5883L_HARD_IRON_OFFSET_X - g_runtimeOffsetX) *
				 (BOARD_HMC5883L_SOFT_IRON_SCALE_X * g_runtimeScaleX);
	correctedY = ((float)rawY - BOARD_HMC5883L_HARD_IRON_OFFSET_Y - g_runtimeOffsetY) *
				 (BOARD_HMC5883L_SOFT_IRON_SCALE_Y * g_runtimeScaleY);
	correctedZ = ((float)rawZ - BOARD_HMC5883L_HARD_IRON_OFFSET_Z - g_runtimeOffsetZ) *
				 (BOARD_HMC5883L_SOFT_IRON_SCALE_Z * g_runtimeScaleZ);

	g_fieldX = correctedX / BOARD_HMC5883L_GAIN_LSB_PER_GAUSS;
	g_fieldY = correctedY / BOARD_HMC5883L_GAIN_LSB_PER_GAUSS;
	g_fieldZ = correctedZ / BOARD_HMC5883L_GAIN_LSB_PER_GAUSS;

	horizontalNorm = sqrtf((g_fieldX * g_fieldX) + (g_fieldY * g_fieldY));
	if (horizontalNorm < BOARD_HMC5883L_MIN_VALID_FIELD_GAUSS)
	{
		g_hmcHeadingValid = 0U;
		return;
	}

	g_headingDeg = atan2f(g_fieldY, g_fieldX) * HMC5883L_RAD_TO_DEG;
	g_headingDeg += BOARD_HMC5883L_HEADING_DECLINATION_DEG;
	g_headingDeg = HMC5883L_Wrap180(g_headingDeg);
	g_hmcHeadingValid = 1U;
}

uint8_t HMC5883L_IsOnline(void)
{
	return g_hmcOnline;
}

uint8_t HMC5883L_IsCalibrated(void)
{
	return g_hmcCalibrated;
}

uint8_t HMC5883L_IsCalibrating(void)
{
	return g_hmcCalibrating;
}

uint8_t HMC5883L_IsHeadingValid(void)
{
	return g_hmcHeadingValid;
}

int16_t HMC5883L_GetRawX(void)
{
	return g_rawX;
}

int16_t HMC5883L_GetRawY(void)
{
	return g_rawY;
}

int16_t HMC5883L_GetRawZ(void)
{
	return g_rawZ;
}

float HMC5883L_GetFieldX(void)
{
	return g_fieldX;
}

float HMC5883L_GetFieldY(void)
{
	return g_fieldY;
}

float HMC5883L_GetFieldZ(void)
{
	return g_fieldZ;
}

float HMC5883L_GetHeadingDeg(void)
{
	return g_headingDeg;
}

void HMC5883L_StartCalibration(void)
{
	HMC5883L_ResetCalibrationBounds();
	HMC5883L_ResetRuntimeCalibration();
	g_hmcCalibrating = 1U;
	g_hmcCalibrated = HMC5883L_HasBoardCalibration();
}

void HMC5883L_StopCalibration(void)
{
	HMC5883L_RecalculateRuntimeCalibration();
	g_hmcCalibrating = 0U;
}

void HMC5883L_ResetCalibration(void)
{
	g_hmcCalibrating = 0U;
	g_hmcCalibrated = HMC5883L_HasBoardCalibration();
	HMC5883L_ResetRuntimeCalibration();
	HMC5883L_ResetCalibrationBounds();
}
