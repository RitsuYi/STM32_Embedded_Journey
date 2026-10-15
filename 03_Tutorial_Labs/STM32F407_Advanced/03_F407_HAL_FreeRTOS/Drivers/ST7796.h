#ifndef __ST7796_H
#define __ST7796_H

#define ST7796_WIDTH   480
#define ST7796_HEIGHT  320

#define ST7796_RED     0xF800
#define ST7796_GREEN   0x07E0
#define ST7796_BLUE    0x001F
#define ST7796_BLACK   0x0000
#define ST7796_WHITE   0xFFFF

#include <stdint.h>

// 发送命令
void ST7796_WriteCommand(uint8_t cmd);

// 发送数据
void ST7796_WriteData(uint8_t data);

// LCD硬件复位
void ST7796_HardwareReset(void);

void ST7796_Init(void);

void ST7796_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

void ST7796_FillScreen(uint16_t color);

#endif
