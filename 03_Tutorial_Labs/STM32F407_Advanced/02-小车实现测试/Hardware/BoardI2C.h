#ifndef __BOARD_I2C_H
#define __BOARD_I2C_H

#include "stm32f4xx.h"

void BoardI2C1_InitOnce(void);
void BoardI2C1_ForceRecover(void);

ErrorStatus BoardI2C1_WriteByte(uint8_t slaveAddress, uint8_t regAddress, uint8_t value);
ErrorStatus BoardI2C1_ReadByte(uint8_t slaveAddress, uint8_t regAddress, uint8_t *value);
ErrorStatus BoardI2C1_ReadBytes(uint8_t slaveAddress, uint8_t regAddress, uint8_t *buffer, uint16_t length);

uint32_t BoardI2C1_GetLastError(void);
uint32_t BoardI2C1_GetErrorCount(void);

#endif
