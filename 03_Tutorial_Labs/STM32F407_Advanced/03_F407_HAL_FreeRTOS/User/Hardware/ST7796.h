#ifndef __ST7796_H
#define __ST7796_H

#define ST7796_WIDTH   480
#define ST7796_HEIGHT  320

#define ST7796_RED     0xF800
#define ST7796_GREEN   0x07E0
#define ST7796_BLUE    0x001F
#define ST7796_BLACK   0x0000
#define ST7796_WHITE   0xFFFF
#define ST7796_YELLOW  0xFFE0

#include <stdint.h>
#include "stm32f4xx_hal.h"

// 发送命令
void ST7796_WriteCommand(uint8_t cmd);

// 发送数据
void ST7796_WriteData(uint8_t data);

// LCD硬件复位
void ST7796_HardwareReset(void);

void ST7796_Init(void);

// 设置显示区域
void ST7796_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

// 填充整屏颜色
void ST7796_FillScreen(uint16_t color);

// 填充矩形区域
void ST7796_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);

/* Native little-endian RGB565, row-major. stride_bytes includes row padding.
 * The caller owns at least (height-1)*stride_bytes + width*2 source bytes.
 * Invalid rectangles are rejected, not clipped. GUI task only; blocking SPI. */
HAL_StatusTypeDef ST7796_WriteRGB565(uint16_t x, uint16_t y,
                                    uint16_t width, uint16_t height,
                                    const uint8_t *pixels, uint32_t stride_bytes);

/* 新LVGL快路径：pixels已经是高字节在前、无行尾填充的RGB565连续矩形。
 * width*height*2必须<=65534；Start成功后源内存直到Finish返回都不可复用。
 * CS及外层递归锁由启动任务持有，必须同一任务调用Finish，ISR只发完成通知。 */
HAL_StatusTypeDef ST7796_StartRGB565DMA(uint16_t x,uint16_t y,uint16_t width,uint16_t height,
                                       const uint8_t *pixels);
HAL_StatusTypeDef ST7796_FinishRGB565DMA(void);

#endif
