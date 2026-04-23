#include "BlueSerial.h"
#include "Serial.h"
#include <stdarg.h>
#include <stdio.h>

#define BLUE_SERIAL_BUFFER_SIZE                 256U
#define BLUE_SERIAL_MAX_FRAME_LENGTH            128U
#define BLUE_SERIAL_MAX_FIELDS                  20U
#define BLUE_SERIAL_MAX_FIELD_LENGTH            31U

static volatile uint8_t g_blueSerialBuffer[BLUE_SERIAL_BUFFER_SIZE];
static volatile uint16_t g_blueSerialPutIndex = 0U;
static volatile uint16_t g_blueSerialGetIndex = 0U;
static volatile uint8_t g_blueSerialFieldCount = 0U;

char BlueSerial_String[BLUE_SERIAL_MAX_FRAME_LENGTH];
char BlueSerial_StringArray[BLUE_SERIAL_MAX_FIELDS][32];

static uint8_t BlueSerial_IsDigit(char ch)
{
	return (uint8_t)((ch >= '0') && (ch <= '9'));
}

static uint8_t BlueSerial_IsAsciiSpace(char ch)
{
	return (uint8_t)((ch == ' ') || (ch == '\t') || (ch == '\r') || (ch == '\n'));
}

static void BlueSerial_TrimField(char *field)
{
	uint8_t startIndex;
	uint8_t endIndex;
	uint8_t writeIndex;

	if (field == 0)
	{
		return;
	}

	startIndex = 0U;
	while ((field[startIndex] != '\0') && (BlueSerial_IsAsciiSpace(field[startIndex]) != 0U))
	{
		startIndex++;
	}

	endIndex = startIndex;
	while (field[endIndex] != '\0')
	{
		endIndex++;
	}

	while ((endIndex > startIndex) && (BlueSerial_IsAsciiSpace(field[endIndex - 1U]) != 0U))
	{
		endIndex--;
	}

	writeIndex = 0U;
	while (startIndex < endIndex)
	{
		field[writeIndex] = field[startIndex];
		writeIndex++;
		startIndex++;
	}

	field[writeIndex] = '\0';
}

static uint8_t BlueSerial_GetSeparatorLength(const char *text, uint16_t index)
{
	char currentChar;
	char prevChar;
	char nextChar;

	if (text == 0)
	{
		return 0U;
	}

	currentChar = text[index];
	prevChar = (index > 0U) ? text[index - 1U] : '\0';
	nextChar = text[index + 1U];

	if (currentChar == ',')
	{
		return 1U;
	}

	if (currentChar == '.')
	{
		/* Keep decimal points inside numbers, but allow [TEL.ON] / [MODE.STOP]. */
		if ((BlueSerial_IsDigit(prevChar) != 0U) && (BlueSerial_IsDigit(nextChar) != 0U))
		{
			return 0U;
		}

		return 1U;
	}

	if ((uint8_t)currentChar == 0xEFU)
	{
		if (text[index + 1U] == '\0')
		{
			return 0U;
		}
		if (text[index + 2U] == '\0')
		{
			return 0U;
		}

		if (((uint8_t)text[index + 1U] == 0xBCU) && ((uint8_t)text[index + 2U] == 0x8CU))
		{
			return 3U;
		}

		if (((uint8_t)text[index + 1U] == 0xBCU) && ((uint8_t)text[index + 2U] == 0x8EU))
		{
			if ((BlueSerial_IsDigit(prevChar) != 0U) && (BlueSerial_IsDigit(text[index + 3U]) != 0U))
			{
				return 0U;
			}

			return 3U;
		}
	}

	if ((uint8_t)currentChar == 0xE3U)
	{
		if (text[index + 1U] == '\0')
		{
			return 0U;
		}
		if (text[index + 2U] == '\0')
		{
			return 0U;
		}

		if (((uint8_t)text[index + 1U] == 0x80U) && ((uint8_t)text[index + 2U] == 0x82U))
		{
			if ((BlueSerial_IsDigit(prevChar) != 0U) && (BlueSerial_IsDigit(text[index + 3U]) != 0U))
			{
				return 0U;
			}

			return 3U;
		}
	}

	return 0U;
}

void BlueSerial_Init(void)
{
	BlueSerial_ClearBuffer();
}

void BlueSerial_InputByte(uint8_t byte)
{
	(void)BlueSerial_Put(byte);
}

void BlueSerial_SendByte(uint8_t byte)
{
	Serial_SendByte(byte);
}

void BlueSerial_SendArray(const uint8_t *array, uint16_t length)
{
	Serial_SendArray(array, length);
}

void BlueSerial_SendString(const char *string)
{
	Serial_SendString(string);
}

void BlueSerial_Printf(const char *format, ...)
{
	char string[160];
	va_list arg;

	va_start(arg, format);
	vsnprintf(string, sizeof(string), format, arg);
	va_end(arg);

	BlueSerial_SendString(string);
}

uint8_t BlueSerial_Put(uint8_t byte)
{
	uint16_t nextIndex;

	nextIndex = (uint16_t)((g_blueSerialPutIndex + 1U) % BLUE_SERIAL_BUFFER_SIZE);
	if (nextIndex == g_blueSerialGetIndex)
	{
		return 1U;
	}

	g_blueSerialBuffer[g_blueSerialPutIndex] = byte;
	g_blueSerialPutIndex = nextIndex;
	return 0U;
}

uint8_t BlueSerial_Get(uint8_t *byte)
{
	if (byte == 0)
	{
		return 1U;
	}

	if (g_blueSerialGetIndex == g_blueSerialPutIndex)
	{
		*byte = 0U;
		return 1U;
	}

	*byte = g_blueSerialBuffer[g_blueSerialGetIndex];
	g_blueSerialGetIndex = (uint16_t)((g_blueSerialGetIndex + 1U) % BLUE_SERIAL_BUFFER_SIZE);
	return 0U;
}

uint16_t BlueSerial_Length(void)
{
	uint16_t putIndex;
	uint16_t getIndex;

	__disable_irq();
	putIndex = g_blueSerialPutIndex;
	getIndex = g_blueSerialGetIndex;
	__enable_irq();

	return (uint16_t)((putIndex + BLUE_SERIAL_BUFFER_SIZE - getIndex) % BLUE_SERIAL_BUFFER_SIZE);
}

void BlueSerial_ClearBuffer(void)
{
	uint8_t fieldIndex;

	__disable_irq();
	g_blueSerialPutIndex = 0U;
	g_blueSerialGetIndex = 0U;
	__enable_irq();

	BlueSerial_String[0] = '\0';
	for (fieldIndex = 0U; fieldIndex < BLUE_SERIAL_MAX_FIELDS; fieldIndex++)
	{
		BlueSerial_StringArray[fieldIndex][0] = '\0';
	}
	g_blueSerialFieldCount = 0U;
}

uint8_t BlueSerial_ReceiveFlag(void)
{
	uint16_t index;
	uint16_t length;
	uint8_t sawLeftBracket;
	uint16_t currentIndex;

	length = BlueSerial_Length();
	currentIndex = g_blueSerialGetIndex;
	sawLeftBracket = 0U;

	for (index = 0U; index < length; index++)
	{
		if (g_blueSerialBuffer[currentIndex] == '[')
		{
			sawLeftBracket = 1U;
		}
		else if ((g_blueSerialBuffer[currentIndex] == ']') && (sawLeftBracket != 0U))
		{
			return 1U;
		}

		currentIndex = (uint16_t)((currentIndex + 1U) % BLUE_SERIAL_BUFFER_SIZE);
	}

	return 0U;
}

void BlueSerial_Receive(void)
{
	uint8_t byte;
	uint16_t stringIndex;
	uint8_t fieldIndex;
	uint8_t fieldCharIndex;
	uint8_t frameStarted;
	uint8_t separatorLength;

	BlueSerial_String[0] = '\0';
	for (fieldIndex = 0U; fieldIndex < BLUE_SERIAL_MAX_FIELDS; fieldIndex++)
	{
		BlueSerial_StringArray[fieldIndex][0] = '\0';
	}
	g_blueSerialFieldCount = 0U;

	stringIndex = 0U;
	frameStarted = 0U;
	while (BlueSerial_Get(&byte) == 0U)
	{
		if (byte == '[')
		{
			frameStarted = 1U;
			stringIndex = 0U;
		}
		else if ((byte == ']') && (frameStarted != 0U))
		{
			break;
		}
		else if (frameStarted != 0U)
		{
			if (stringIndex < (BLUE_SERIAL_MAX_FRAME_LENGTH - 1U))
			{
				BlueSerial_String[stringIndex] = (char)byte;
				stringIndex++;
			}
		}
	}

	if (frameStarted == 0U)
	{
		return;
	}

	BlueSerial_String[stringIndex] = '\0';
	fieldIndex = 0U;
	fieldCharIndex = 0U;
	for (stringIndex = 0U; BlueSerial_String[stringIndex] != '\0';)
	{
		if (fieldIndex >= BLUE_SERIAL_MAX_FIELDS)
		{
			break;
		}

		separatorLength = BlueSerial_GetSeparatorLength(BlueSerial_String, stringIndex);
		if (separatorLength != 0U)
		{
			BlueSerial_StringArray[fieldIndex][fieldCharIndex] = '\0';
			BlueSerial_TrimField(BlueSerial_StringArray[fieldIndex]);
			fieldIndex++;
			fieldCharIndex = 0U;
			stringIndex += separatorLength;
		}
		else
		{
			if (fieldCharIndex < BLUE_SERIAL_MAX_FIELD_LENGTH)
			{
				BlueSerial_StringArray[fieldIndex][fieldCharIndex] = BlueSerial_String[stringIndex];
				fieldCharIndex++;
			}

			stringIndex++;
		}
	}

	if ((frameStarted != 0U) && (fieldIndex < BLUE_SERIAL_MAX_FIELDS))
	{
		BlueSerial_StringArray[fieldIndex][fieldCharIndex] = '\0';
		BlueSerial_TrimField(BlueSerial_StringArray[fieldIndex]);
		g_blueSerialFieldCount = (uint8_t)(fieldIndex + 1U);
	}
}

uint8_t BlueSerial_GetFieldCount(void)
{
	return g_blueSerialFieldCount;
}
