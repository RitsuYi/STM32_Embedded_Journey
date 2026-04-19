#include "Serial.h"
#include "BlueSerial.h"
#include "BoardConfig.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define SERIAL_TX_BUFFER_SIZE                    1024U

static volatile uint8_t g_serialRxData = 0U;
static volatile uint8_t g_serialRxFlag = 0U;
static volatile uint8_t g_serialTxBuffer[SERIAL_TX_BUFFER_SIZE];
static volatile uint16_t g_serialTxPutIndex = 0U;
static volatile uint16_t g_serialTxGetIndex = 0U;

static uint16_t Serial_GetTxFreeSpaceLocked(uint16_t putIndex, uint16_t getIndex)
{
	return (uint16_t)((getIndex + SERIAL_TX_BUFFER_SIZE - putIndex - 1U) % SERIAL_TX_BUFFER_SIZE);
}

static void Serial_KickTxLocked(void)
{
	if (g_serialTxGetIndex == g_serialTxPutIndex)
	{
		USART_ITConfig(BOARD_BLUETOOTH_USART, USART_IT_TXE, DISABLE);
		return;
	}

	if (USART_GetFlagStatus(BOARD_BLUETOOTH_USART, USART_FLAG_TXE) == SET)
	{
		USART_SendData(BOARD_BLUETOOTH_USART, g_serialTxBuffer[g_serialTxGetIndex]);
		g_serialTxGetIndex = (uint16_t)((g_serialTxGetIndex + 1U) % SERIAL_TX_BUFFER_SIZE);
	}

	if (g_serialTxGetIndex != g_serialTxPutIndex)
	{
		USART_ITConfig(BOARD_BLUETOOTH_USART, USART_IT_TXE, ENABLE);
	}
	else
	{
		USART_ITConfig(BOARD_BLUETOOTH_USART, USART_IT_TXE, DISABLE);
	}
}

static uint8_t Serial_EnqueueTxArray(const uint8_t *array, uint16_t length)
{
	uint16_t index;
	uint16_t putIndex;
	uint16_t getIndex;

	if ((array == 0) || (length == 0U))
	{
		return 0U;
	}

	__disable_irq();
	putIndex = g_serialTxPutIndex;
	getIndex = g_serialTxGetIndex;
	if (length > Serial_GetTxFreeSpaceLocked(putIndex, getIndex))
	{
		__enable_irq();
		return 1U;
	}

	for (index = 0U; index < length; index++)
	{
		g_serialTxBuffer[putIndex] = array[index];
		putIndex = (uint16_t)((putIndex + 1U) % SERIAL_TX_BUFFER_SIZE);
	}

	g_serialTxPutIndex = putIndex;
	Serial_KickTxLocked();
	__enable_irq();

	return 0U;
}

void Serial_Init(void)
{
	GPIO_InitTypeDef gpioInitStructure;
	USART_InitTypeDef usartInitStructure;
	NVIC_InitTypeDef nvicInitStructure;

	RCC_APB1PeriphClockCmd(BOARD_BLUETOOTH_USART_RCC, ENABLE);
	RCC_AHB1PeriphClockCmd(BOARD_BLUETOOTH_GPIO_RCC, ENABLE);

	g_serialRxData = 0U;
	g_serialRxFlag = 0U;
	g_serialTxPutIndex = 0U;
	g_serialTxGetIndex = 0U;

	GPIO_PinAFConfig(BOARD_BLUETOOTH_GPIO_PORT,
					 BOARD_BLUETOOTH_TX_PINSOURCE,
					 BOARD_BLUETOOTH_GPIO_AF);
	GPIO_PinAFConfig(BOARD_BLUETOOTH_GPIO_PORT,
					 BOARD_BLUETOOTH_RX_PINSOURCE,
					 BOARD_BLUETOOTH_GPIO_AF);

	gpioInitStructure.GPIO_Mode = GPIO_Mode_AF;
	gpioInitStructure.GPIO_OType = GPIO_OType_PP;
	gpioInitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	gpioInitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	gpioInitStructure.GPIO_Pin = BOARD_BLUETOOTH_TX_PIN | BOARD_BLUETOOTH_RX_PIN;
	GPIO_Init(BOARD_BLUETOOTH_GPIO_PORT, &gpioInitStructure);

	USART_StructInit(&usartInitStructure);
	usartInitStructure.USART_BaudRate = BOARD_BLUETOOTH_BAUDRATE;
	usartInitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usartInitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	usartInitStructure.USART_Parity = USART_Parity_No;
	usartInitStructure.USART_StopBits = USART_StopBits_1;
	usartInitStructure.USART_WordLength = USART_WordLength_8b;
	USART_Init(BOARD_BLUETOOTH_USART, &usartInitStructure);

	USART_ITConfig(BOARD_BLUETOOTH_USART, USART_IT_RXNE, ENABLE);
	USART_ITConfig(BOARD_BLUETOOTH_USART, USART_IT_TXE, DISABLE);

	nvicInitStructure.NVIC_IRQChannel = BOARD_BLUETOOTH_IRQ;
	nvicInitStructure.NVIC_IRQChannelCmd = ENABLE;
	nvicInitStructure.NVIC_IRQChannelPreemptionPriority = 2U;
	nvicInitStructure.NVIC_IRQChannelSubPriority = 0U;
	NVIC_Init(&nvicInitStructure);

	USART_Cmd(BOARD_BLUETOOTH_USART, ENABLE);
}

void Serial_SendByte(uint8_t byte)
{
	(void)Serial_EnqueueTxArray(&byte, 1U);
}

void Serial_SendArray(const uint8_t *array, uint16_t length)
{
	(void)Serial_EnqueueTxArray(array, length);
}

void Serial_SendString(const char *string)
{
	size_t length;

	if (string == 0)
	{
		return;
	}

	length = strlen(string);
	if (length == 0U)
	{
		return;
	}

	(void)Serial_EnqueueTxArray((const uint8_t *)string, (uint16_t)length);
}

void Serial_Printf(const char *format, ...)
{
	char string[128];
	va_list arg;

	va_start(arg, format);
	vsnprintf(string, sizeof(string), format, arg);
	va_end(arg);

	Serial_SendString(string);
}

uint8_t Serial_GetRxFlag(void)
{
	if (g_serialRxFlag != 0U)
	{
		g_serialRxFlag = 0U;
		return 1U;
	}

	return 0U;
}

uint8_t Serial_GetRxData(void)
{
	return g_serialRxData;
}

void USART3_IRQHandler(void)
{
	if (USART_GetITStatus(BOARD_BLUETOOTH_USART, USART_IT_RXNE) == SET)
	{
		g_serialRxData = (uint8_t)USART_ReceiveData(BOARD_BLUETOOTH_USART);
		g_serialRxFlag = 1U;
		BlueSerial_InputByte(g_serialRxData);
		USART_ClearITPendingBit(BOARD_BLUETOOTH_USART, USART_IT_RXNE);
	}

	if (USART_GetITStatus(BOARD_BLUETOOTH_USART, USART_IT_TXE) == SET)
	{
		if (g_serialTxGetIndex != g_serialTxPutIndex)
		{
			USART_SendData(BOARD_BLUETOOTH_USART, g_serialTxBuffer[g_serialTxGetIndex]);
			g_serialTxGetIndex = (uint16_t)((g_serialTxGetIndex + 1U) % SERIAL_TX_BUFFER_SIZE);
		}
		else
		{
			USART_ITConfig(BOARD_BLUETOOTH_USART, USART_IT_TXE, DISABLE);
		}
	}
}
