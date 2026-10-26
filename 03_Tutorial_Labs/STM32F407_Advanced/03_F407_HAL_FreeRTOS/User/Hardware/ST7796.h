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

/* GUI任务调用一次初始化DMA2 Stream0；初始化成功返回HAL_OK。
 * 不改动CubeMX的USART2 DMA配置，禁止其他模块占用同一条DMA流。 */
HAL_StatusTypeDef ST7796_DMAInit(void);
/* 由Core中DMA2 Stream0的中断入口转交HAL处理，业务层不要主动调用。 */
void ST7796_DMAIRQHandler(void);

/* 异步发送原生RGB565矩形：Start返回HAL_OK表示已启动，CPU可绘制另一块缓冲。
 * pixels必须半字对齐、完整落在普通SRAM，不能放CCM；stride_bytes是两行起点距离。
 * 调用者保证至少(height-1)*stride_bytes + width*2字节，等待期间不得修改/释放源数据。
 * 同一GUI任务必须配对调用Wait；成功、错误或超时安全停流后才可复用源缓冲。
 * 在途重复Start返回HAL_BUSY且不影响原传输；等待期间不得调用其他LCD写入接口。
 * 两个LVGL缓冲轮换构成绘图/发送流水线，不是DMA硬件DBM或两个整屏显存。 */
HAL_StatusTypeDef ST7796_WriteRGB565Start(uint16_t x, uint16_t y,
                                        uint16_t width, uint16_t height,
                                        const uint8_t *pixels, uint32_t stride_bytes);
HAL_StatusTypeDef ST7796_WriteRGB565Wait(void);

/* 将小端RGB565矩形按行写入屏幕，stride_bytes是相邻两行起点的字节距离。
 * 调用者提供至少(height-1)*stride_bytes + width*2字节，返回前不可改写源数据。
 * 非法区域直接拒绝，不自动裁剪；只允许GUI任务调用。
 * 这是CPU同步兼容接口；LVGL使用上面的Start/Wait异步接口。
 * HAL_OK仅表示软件写入结束，不提供LCD硬件读回确认。 */
HAL_StatusTypeDef ST7796_WriteRGB565(uint16_t x, uint16_t y,
                                    uint16_t width, uint16_t height,
                                    const uint8_t *pixels, uint32_t stride_bytes);

#endif
