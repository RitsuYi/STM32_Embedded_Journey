#ifndef __ST7796_H
#define __ST7796_H

#include <stdint.h>

// 发送命令
void ST7796_WriteCommand(uint8_t cmd);

// 发送数据
void ST7796_WriteData(uint8_t data);

// LCD硬件复位
void ST7796_HardwareReset(void);

void ST7796_Init(void);

#endif
