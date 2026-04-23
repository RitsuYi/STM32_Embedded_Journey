#include "BoardI2C.h"
#include "BoardConfig.h"

#define BOARD_I2C1_ERROR_BUSY_TIMEOUT           0x40000000UL
#define BOARD_I2C1_ERROR_EVENT_TIMEOUT          0x80000000UL

static volatile uint8_t g_boardI2C1Inited = 0U;
static volatile uint32_t g_boardI2C1LastError = 0U;
static volatile uint32_t g_boardI2C1ErrorCount = 0U;

static void BoardI2C_GPIOClockCmd(GPIO_TypeDef *gpiox, FunctionalState newState)
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

static void BoardI2C1_ClearErrorFlags(void)
{
	if (I2C_GetFlagStatus(OLED_I2C_INSTANCE, I2C_FLAG_AF) == SET)
	{
		I2C_ClearFlag(OLED_I2C_INSTANCE, I2C_FLAG_AF);
	}

	if (I2C_GetFlagStatus(OLED_I2C_INSTANCE, I2C_FLAG_BERR) == SET)
	{
		I2C_ClearFlag(OLED_I2C_INSTANCE, I2C_FLAG_BERR);
	}

	if (I2C_GetFlagStatus(OLED_I2C_INSTANCE, I2C_FLAG_ARLO) == SET)
	{
		I2C_ClearFlag(OLED_I2C_INSTANCE, I2C_FLAG_ARLO);
	}

	if (I2C_GetFlagStatus(OLED_I2C_INSTANCE, I2C_FLAG_OVR) == SET)
	{
		I2C_ClearFlag(OLED_I2C_INSTANCE, I2C_FLAG_OVR);
	}
}

static void BoardI2C1_ConfigPins(void)
{
	GPIO_InitTypeDef gpioInitStructure;

	BoardI2C_GPIOClockCmd(OLED_SCL_PORT, ENABLE);
	if (OLED_SDA_PORT != OLED_SCL_PORT)
	{
		BoardI2C_GPIOClockCmd(OLED_SDA_PORT, ENABLE);
	}
	RCC_APB1PeriphClockCmd(OLED_I2C_RCC_APB1_PERIPH, ENABLE);

	GPIO_PinAFConfig(OLED_SCL_PORT, OLED_SCL_PINSOURCE, OLED_I2C_GPIO_AF);
	GPIO_PinAFConfig(OLED_SDA_PORT, OLED_SDA_PINSOURCE, OLED_I2C_GPIO_AF);

	gpioInitStructure.GPIO_Mode = GPIO_Mode_AF;
	gpioInitStructure.GPIO_OType = GPIO_OType_OD;
	gpioInitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	gpioInitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	gpioInitStructure.GPIO_Pin = OLED_SCL_PIN;
	GPIO_Init(OLED_SCL_PORT, &gpioInitStructure);
	gpioInitStructure.GPIO_Pin = OLED_SDA_PIN;
	GPIO_Init(OLED_SDA_PORT, &gpioInitStructure);
}

static void BoardI2C1_RecoverBus(void)
{
	I2C_InitTypeDef i2cInitStructure;

	BoardI2C1_ConfigPins();

	I2C_Cmd(OLED_I2C_INSTANCE, DISABLE);
	I2C_DeInit(OLED_I2C_INSTANCE);

	i2cInitStructure.I2C_ClockSpeed = OLED_I2C_CLOCK_SPEED;
	i2cInitStructure.I2C_Mode = I2C_Mode_I2C;
	i2cInitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
	i2cInitStructure.I2C_OwnAddress1 = 0x00U;
	i2cInitStructure.I2C_Ack = I2C_Ack_Enable;
	i2cInitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_Init(OLED_I2C_INSTANCE, &i2cInitStructure);
	I2C_Cmd(OLED_I2C_INSTANCE, ENABLE);

	g_boardI2C1Inited = 1U;
}

static ErrorStatus BoardI2C1_AbortTransfer(uint32_t errorCode)
{
	g_boardI2C1LastError = errorCode;
	g_boardI2C1ErrorCount++;

	I2C_GenerateSTOP(OLED_I2C_INSTANCE, ENABLE);
	BoardI2C1_ClearErrorFlags();
	BoardI2C1_RecoverBus();
	return ERROR;
}

static ErrorStatus BoardI2C1_WaitBusyReset(void)
{
	uint32_t timeout;

	timeout = OLED_I2C_TIMEOUT;
	while (I2C_GetFlagStatus(OLED_I2C_INSTANCE, I2C_FLAG_BUSY) == SET)
	{
		if (timeout-- == 0U)
		{
			return BoardI2C1_AbortTransfer(BOARD_I2C1_ERROR_BUSY_TIMEOUT);
		}
	}

	return SUCCESS;
}

static ErrorStatus BoardI2C1_WaitEvent(uint32_t event)
{
	uint32_t timeout;

	timeout = OLED_I2C_TIMEOUT;
	while (I2C_CheckEvent(OLED_I2C_INSTANCE, event) != SUCCESS)
	{
		if (I2C_GetFlagStatus(OLED_I2C_INSTANCE, I2C_FLAG_AF) == SET)
		{
			return BoardI2C1_AbortTransfer(I2C_FLAG_AF);
		}

		if (I2C_GetFlagStatus(OLED_I2C_INSTANCE, I2C_FLAG_BERR) == SET)
		{
			return BoardI2C1_AbortTransfer(I2C_FLAG_BERR);
		}

		if (I2C_GetFlagStatus(OLED_I2C_INSTANCE, I2C_FLAG_ARLO) == SET)
		{
			return BoardI2C1_AbortTransfer(I2C_FLAG_ARLO);
		}

		if (I2C_GetFlagStatus(OLED_I2C_INSTANCE, I2C_FLAG_OVR) == SET)
		{
			return BoardI2C1_AbortTransfer(I2C_FLAG_OVR);
		}

		if (timeout-- == 0U)
		{
			return BoardI2C1_AbortTransfer(BOARD_I2C1_ERROR_EVENT_TIMEOUT);
		}
	}

	return SUCCESS;
}

void BoardI2C1_InitOnce(void)
{
	if (g_boardI2C1Inited != 0U)
	{
		return;
	}

	g_boardI2C1LastError = 0U;
	g_boardI2C1ErrorCount = 0U;
	BoardI2C1_RecoverBus();
}

void BoardI2C1_ForceRecover(void)
{
	BoardI2C1_RecoverBus();
}

ErrorStatus BoardI2C1_WriteByte(uint8_t slaveAddress, uint8_t regAddress, uint8_t value)
{
	BoardI2C1_InitOnce();
	g_boardI2C1LastError = 0U;

	if (BoardI2C1_WaitBusyReset() != SUCCESS)
	{
		return ERROR;
	}

	I2C_GenerateSTART(OLED_I2C_INSTANCE, ENABLE);
	if (BoardI2C1_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
	{
		return ERROR;
	}

	I2C_Send7bitAddress(OLED_I2C_INSTANCE, slaveAddress, I2C_Direction_Transmitter);
	if (BoardI2C1_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != SUCCESS)
	{
		return ERROR;
	}

	I2C_SendData(OLED_I2C_INSTANCE, regAddress);
	if (BoardI2C1_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
	{
		return ERROR;
	}

	I2C_SendData(OLED_I2C_INSTANCE, value);
	if (BoardI2C1_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
	{
		return ERROR;
	}

	I2C_GenerateSTOP(OLED_I2C_INSTANCE, ENABLE);
	return SUCCESS;
}

ErrorStatus BoardI2C1_ReadByte(uint8_t slaveAddress, uint8_t regAddress, uint8_t *value)
{
	return BoardI2C1_ReadBytes(slaveAddress, regAddress, value, 1U);
}

ErrorStatus BoardI2C1_ReadBytes(uint8_t slaveAddress, uint8_t regAddress, uint8_t *buffer, uint16_t length)
{
	if ((buffer == 0) || (length == 0U))
	{
		return ERROR;
	}

	BoardI2C1_InitOnce();
	g_boardI2C1LastError = 0U;

	if (BoardI2C1_WaitBusyReset() != SUCCESS)
	{
		return ERROR;
	}

	I2C_AcknowledgeConfig(OLED_I2C_INSTANCE, ENABLE);

	I2C_GenerateSTART(OLED_I2C_INSTANCE, ENABLE);
	if (BoardI2C1_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
	{
		I2C_AcknowledgeConfig(OLED_I2C_INSTANCE, ENABLE);
		return ERROR;
	}

	I2C_Send7bitAddress(OLED_I2C_INSTANCE, slaveAddress, I2C_Direction_Transmitter);
	if (BoardI2C1_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != SUCCESS)
	{
		I2C_AcknowledgeConfig(OLED_I2C_INSTANCE, ENABLE);
		return ERROR;
	}

	I2C_SendData(OLED_I2C_INSTANCE, regAddress);
	if (BoardI2C1_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
	{
		I2C_AcknowledgeConfig(OLED_I2C_INSTANCE, ENABLE);
		return ERROR;
	}

	I2C_GenerateSTART(OLED_I2C_INSTANCE, ENABLE);
	if (BoardI2C1_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
	{
		I2C_AcknowledgeConfig(OLED_I2C_INSTANCE, ENABLE);
		return ERROR;
	}

	I2C_Send7bitAddress(OLED_I2C_INSTANCE, slaveAddress, I2C_Direction_Receiver);
	if (BoardI2C1_WaitEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) != SUCCESS)
	{
		I2C_AcknowledgeConfig(OLED_I2C_INSTANCE, ENABLE);
		return ERROR;
	}

	while (length > 0U)
	{
		if (length == 1U)
		{
			I2C_AcknowledgeConfig(OLED_I2C_INSTANCE, DISABLE);
			I2C_GenerateSTOP(OLED_I2C_INSTANCE, ENABLE);
		}

		if (BoardI2C1_WaitEvent(I2C_EVENT_MASTER_BYTE_RECEIVED) != SUCCESS)
		{
			I2C_AcknowledgeConfig(OLED_I2C_INSTANCE, ENABLE);
			return ERROR;
		}

		*buffer = I2C_ReceiveData(OLED_I2C_INSTANCE);
		buffer++;
		length--;
	}

	I2C_AcknowledgeConfig(OLED_I2C_INSTANCE, ENABLE);
	return SUCCESS;
}

uint32_t BoardI2C1_GetLastError(void)
{
	return g_boardI2C1LastError;
}

uint32_t BoardI2C1_GetErrorCount(void)
{
	return g_boardI2C1ErrorCount;
}
