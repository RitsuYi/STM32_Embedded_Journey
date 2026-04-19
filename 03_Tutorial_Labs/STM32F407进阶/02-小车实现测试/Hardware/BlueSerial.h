#ifndef __BLUE_SERIAL_H
#define __BLUE_SERIAL_H

#include "stm32f4xx.h"

extern char BlueSerial_String[];
extern char BlueSerial_StringArray[][32];

void BlueSerial_Init(void);
void BlueSerial_InputByte(uint8_t byte);
void BlueSerial_SendByte(uint8_t byte);
void BlueSerial_SendArray(const uint8_t *array, uint16_t length);
void BlueSerial_SendString(const char *string);
void BlueSerial_Printf(const char *format, ...);

uint8_t BlueSerial_Put(uint8_t byte);
uint8_t BlueSerial_Get(uint8_t *byte);
uint16_t BlueSerial_Length(void);
void BlueSerial_ClearBuffer(void);
uint8_t BlueSerial_ReceiveFlag(void);
void BlueSerial_Receive(void);
uint8_t BlueSerial_GetFieldCount(void);

#endif
