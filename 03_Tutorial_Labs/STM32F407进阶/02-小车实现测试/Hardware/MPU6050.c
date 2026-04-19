#include "MPU6050.h"
#include "BoardConfig.h"
#include "Delay.h"

#define MPU6050_REG_SMPLRT_DIV                 0x19U
#define MPU6050_REG_CONFIG                     0x1AU
#define MPU6050_REG_GYRO_CONFIG                0x1BU
#define MPU6050_REG_PWR_MGMT_1                 0x6BU
#define MPU6050_REG_WHO_AM_I                   0x75U
#define MPU6050_REG_GYRO_ZOUT_H                0x47U
#define MPU6050_REG_GYRO_ZOUT_L                0x48U

static volatile uint8_t g_mpuOnline = 0U;
static volatile uint8_t g_mpuCalibrated = 0U;
static volatile int16_t g_rawGyroZ = 0;
static volatile float g_gyroBiasZDps = 0.0f;
static volatile float g_gyroZDps = 0.0f;
static volatile float g_yawDeg = 0.0f;

static void MPU6050_GPIOClockCmd(GPIO_TypeDef *gpiox, FunctionalState newState)
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

static void MPU6050_BusInit(void)
{
	GPIO_InitTypeDef gpioInitStructure;
	I2C_InitTypeDef i2cInitStructure;

	MPU6050_GPIOClockCmd(BOARD_MPU6050_SCL_PORT, ENABLE);
	if (BOARD_MPU6050_SDA_PORT != BOARD_MPU6050_SCL_PORT)
	{
		MPU6050_GPIOClockCmd(BOARD_MPU6050_SDA_PORT, ENABLE);
	}
	RCC_APB1PeriphClockCmd(BOARD_MPU6050_I2C_RCC_APB1_PERIPH, ENABLE);

	GPIO_PinAFConfig(BOARD_MPU6050_SCL_PORT,
					 BOARD_MPU6050_SCL_PINSOURCE,
					 BOARD_MPU6050_I2C_GPIO_AF);
	GPIO_PinAFConfig(BOARD_MPU6050_SDA_PORT,
					 BOARD_MPU6050_SDA_PINSOURCE,
					 BOARD_MPU6050_I2C_GPIO_AF);

	gpioInitStructure.GPIO_Mode = GPIO_Mode_AF;
	gpioInitStructure.GPIO_OType = GPIO_OType_OD;
	gpioInitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	gpioInitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	gpioInitStructure.GPIO_Pin = BOARD_MPU6050_SCL_PIN;
	GPIO_Init(BOARD_MPU6050_SCL_PORT, &gpioInitStructure);
	gpioInitStructure.GPIO_Pin = BOARD_MPU6050_SDA_PIN;
	GPIO_Init(BOARD_MPU6050_SDA_PORT, &gpioInitStructure);

	I2C_Cmd(BOARD_MPU6050_I2C_INSTANCE, DISABLE);
	I2C_DeInit(BOARD_MPU6050_I2C_INSTANCE);

	i2cInitStructure.I2C_ClockSpeed = OLED_I2C_CLOCK_SPEED;
	i2cInitStructure.I2C_Mode = I2C_Mode_I2C;
	i2cInitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
	i2cInitStructure.I2C_OwnAddress1 = 0x00U;
	i2cInitStructure.I2C_Ack = I2C_Ack_Enable;
	i2cInitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_Init(BOARD_MPU6050_I2C_INSTANCE, &i2cInitStructure);
	I2C_Cmd(BOARD_MPU6050_I2C_INSTANCE, ENABLE);
}

static ErrorStatus MPU6050_WaitBusyReset(void)
{
	uint32_t timeout;

	timeout = OLED_I2C_TIMEOUT;
	while (I2C_GetFlagStatus(BOARD_MPU6050_I2C_INSTANCE, I2C_FLAG_BUSY) == SET)
	{
		if (timeout-- == 0U)
		{
			MPU6050_BusInit();
			return ERROR;
		}
	}

	return SUCCESS;
}

static ErrorStatus MPU6050_WaitEvent(uint32_t event)
{
	uint32_t timeout;

	timeout = OLED_I2C_TIMEOUT;
	while (I2C_CheckEvent(BOARD_MPU6050_I2C_INSTANCE, event) != SUCCESS)
	{
		if (timeout-- == 0U)
		{
			I2C_GenerateSTOP(BOARD_MPU6050_I2C_INSTANCE, ENABLE);
			MPU6050_BusInit();
			return ERROR;
		}
	}

	return SUCCESS;
}

static ErrorStatus MPU6050_WriteRegister(uint8_t regAddress, uint8_t value)
{
	if (MPU6050_WaitBusyReset() != SUCCESS)
	{
		return ERROR;
	}

	I2C_GenerateSTART(BOARD_MPU6050_I2C_INSTANCE, ENABLE);
	if (MPU6050_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
	{
		return ERROR;
	}

	I2C_Send7bitAddress(BOARD_MPU6050_I2C_INSTANCE,
						BOARD_MPU6050_I2C_ADDRESS,
						I2C_Direction_Transmitter);
	if (MPU6050_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != SUCCESS)
	{
		return ERROR;
	}

	I2C_SendData(BOARD_MPU6050_I2C_INSTANCE, regAddress);
	if (MPU6050_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
	{
		return ERROR;
	}

	I2C_SendData(BOARD_MPU6050_I2C_INSTANCE, value);
	if (MPU6050_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
	{
		return ERROR;
	}

	I2C_GenerateSTOP(BOARD_MPU6050_I2C_INSTANCE, ENABLE);
	return SUCCESS;
}

static ErrorStatus MPU6050_ReadRegister(uint8_t regAddress, uint8_t *value)
{
	if (value == 0)
	{
		return ERROR;
	}

	if (MPU6050_WaitBusyReset() != SUCCESS)
	{
		return ERROR;
	}

	I2C_AcknowledgeConfig(BOARD_MPU6050_I2C_INSTANCE, ENABLE);

	I2C_GenerateSTART(BOARD_MPU6050_I2C_INSTANCE, ENABLE);
	if (MPU6050_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
	{
		return ERROR;
	}

	I2C_Send7bitAddress(BOARD_MPU6050_I2C_INSTANCE,
						BOARD_MPU6050_I2C_ADDRESS,
						I2C_Direction_Transmitter);
	if (MPU6050_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != SUCCESS)
	{
		return ERROR;
	}

	I2C_SendData(BOARD_MPU6050_I2C_INSTANCE, regAddress);
	if (MPU6050_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
	{
		return ERROR;
	}

	I2C_GenerateSTART(BOARD_MPU6050_I2C_INSTANCE, ENABLE);
	if (MPU6050_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
	{
		return ERROR;
	}

	I2C_AcknowledgeConfig(BOARD_MPU6050_I2C_INSTANCE, DISABLE);
	I2C_Send7bitAddress(BOARD_MPU6050_I2C_INSTANCE,
						BOARD_MPU6050_I2C_ADDRESS,
						I2C_Direction_Receiver);
	if (MPU6050_WaitEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) != SUCCESS)
	{
		I2C_AcknowledgeConfig(BOARD_MPU6050_I2C_INSTANCE, ENABLE);
		return ERROR;
	}

	I2C_GenerateSTOP(BOARD_MPU6050_I2C_INSTANCE, ENABLE);

	if (MPU6050_WaitEvent(I2C_EVENT_MASTER_BYTE_RECEIVED) != SUCCESS)
	{
		I2C_AcknowledgeConfig(BOARD_MPU6050_I2C_INSTANCE, ENABLE);
		return ERROR;
	}

	*value = I2C_ReceiveData(BOARD_MPU6050_I2C_INSTANCE);
	I2C_AcknowledgeConfig(BOARD_MPU6050_I2C_INSTANCE, ENABLE);
	return SUCCESS;
}

static int16_t MPU6050_ReadRawGyroZ(void)
{
	uint8_t highByte;
	uint8_t lowByte;

	if ((MPU6050_ReadRegister(MPU6050_REG_GYRO_ZOUT_H, &highByte) != SUCCESS) ||
		(MPU6050_ReadRegister(MPU6050_REG_GYRO_ZOUT_L, &lowByte) != SUCCESS))
	{
		g_mpuOnline = 0U;
		return g_rawGyroZ;
	}

	return (int16_t)((int16_t)((highByte << 8) | lowByte));
}

static float MPU6050_ConvertRawGyroZToDps(int16_t rawGyroZ)
{
	float gyroZDps;

	gyroZDps = (float)rawGyroZ / BOARD_MPU6050_GYRO_SENSITIVITY_LSB;
	if (BOARD_MPU6050_GYRO_Z_REVERSE != 0U)
	{
		gyroZDps = -gyroZDps;
	}

	return gyroZDps;
}

static float MPU6050_WrapYaw(float yawDeg)
{
	while (yawDeg > 180.0f)
	{
		yawDeg -= 360.0f;
	}

	while (yawDeg < -180.0f)
	{
		yawDeg += 360.0f;
	}

	return yawDeg;
}

void MPU6050_StartGyroBiasCalibration(void)
{
	uint16_t sampleIndex;
	float biasSum;
	int16_t rawGyroZ;

	if (g_mpuOnline == 0U)
	{
		g_mpuCalibrated = 0U;
		g_gyroBiasZDps = 0.0f;
		return;
	}

	biasSum = 0.0f;
	for (sampleIndex = 0U; sampleIndex < BOARD_MPU6050_CALIBRATION_SAMPLES; sampleIndex++)
	{
		rawGyroZ = MPU6050_ReadRawGyroZ();
		biasSum += MPU6050_ConvertRawGyroZToDps(rawGyroZ);
		Delay_ms(BOARD_MPU6050_CALIBRATION_DELAY_MS);
	}

	g_gyroBiasZDps = biasSum / (float)BOARD_MPU6050_CALIBRATION_SAMPLES;
	g_gyroZDps = 0.0f;
	g_yawDeg = 0.0f;
	g_mpuCalibrated = 1U;
}

void MPU6050_Init(void)
{
	uint8_t whoAmI;

	g_mpuOnline = 0U;
	g_mpuCalibrated = 0U;
	g_rawGyroZ = 0;
	g_gyroBiasZDps = 0.0f;
	g_gyroZDps = 0.0f;
	g_yawDeg = 0.0f;

	MPU6050_BusInit();
	Delay_ms(20U);

	if (MPU6050_ReadRegister(MPU6050_REG_WHO_AM_I, &whoAmI) != SUCCESS)
	{
		return;
	}

	if (whoAmI != BOARD_MPU6050_WHO_AM_I_EXPECTED)
	{
		return;
	}

	g_mpuOnline = 1U;

	if (MPU6050_WriteRegister(MPU6050_REG_PWR_MGMT_1, 0x00U) != SUCCESS)
	{
		g_mpuOnline = 0U;
		return;
	}

	(void)MPU6050_WriteRegister(MPU6050_REG_SMPLRT_DIV, 0x07U);
	(void)MPU6050_WriteRegister(MPU6050_REG_CONFIG, 0x03U);
	(void)MPU6050_WriteRegister(MPU6050_REG_GYRO_CONFIG, 0x00U);

	Delay_ms(50U);
	MPU6050_StartGyroBiasCalibration();
}

void MPU6050_SampleTask(void)
{
	float newGyroZDps;

	if (g_mpuOnline == 0U)
	{
		return;
	}

	g_rawGyroZ = MPU6050_ReadRawGyroZ();
	newGyroZDps = MPU6050_ConvertRawGyroZToDps(g_rawGyroZ) - g_gyroBiasZDps;
	g_gyroZDps = (BOARD_MPU6050_GYRO_LPF_ALPHA * newGyroZDps) +
				((1.0f - BOARD_MPU6050_GYRO_LPF_ALPHA) * g_gyroZDps);

	g_yawDeg += g_gyroZDps * ((float)BOARD_MPU6050_SAMPLE_MS / 1000.0f);
	g_yawDeg = MPU6050_WrapYaw(g_yawDeg);
}

void MPU6050_ResetYaw(void)
{
	g_yawDeg = 0.0f;
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

float MPU6050_GetGyroZDps(void)
{
	return g_gyroZDps;
}

float MPU6050_GetYawDeg(void)
{
	return g_yawDeg;
}

float MPU6050_GetGyroBiasZDps(void)
{
	return g_gyroBiasZDps;
}
