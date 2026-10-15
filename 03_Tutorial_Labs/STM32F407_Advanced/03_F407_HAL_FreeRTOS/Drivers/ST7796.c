#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "spi.h"
#include "ST7796.h"

void ST7796_WriteCommand(uint8_t cmd)
{
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

void ST7796_WriteData(uint8_t data)
{
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
    HAL_SPI_Transmit(&hspi1, &data, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

void ST7796_HardwareReset(void)
{
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);

  HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_RESET);

  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
  vTaskDelay(pdMS_TO_TICKS(10));
  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
  vTaskDelay(pdMS_TO_TICKS(20));
  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
  vTaskDelay(pdMS_TO_TICKS(120));
}


void ST7796_Init(void)
{
    // 硬件复位
    ST7796_HardwareReset();

    // 退出睡眠模式
    ST7796_WriteCommand(0x11);
    vTaskDelay(pdMS_TO_TICKS(120) + 1U);

    // RGB565颜色格式
    ST7796_WriteCommand(0x3A);
    ST7796_WriteData(0x55);

    // 横屏显示方向，后续通过色块验证
    ST7796_WriteCommand(0x36);
    ST7796_WriteData(0x28);

    // 开启显示
    ST7796_WriteCommand(0x29);
    vTaskDelay(pdMS_TO_TICKS(20) + 1U);

    // 打开背光
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);
}

//设置显示区域
void ST7796_SetAddressWindow(uint16_t x0, uint16_t y0,
                             uint16_t x1, uint16_t y1)
{
    // 设置列地址
    ST7796_WriteCommand(0x2A);
    ST7796_WriteData(x0 >> 8);
    ST7796_WriteData(x0 & 0xFF);
    ST7796_WriteData(x1 >> 8);
    ST7796_WriteData(x1 & 0xFF);

    // 设置行地址
    ST7796_WriteCommand(0x2B);
    ST7796_WriteData(y0 >> 8);
    ST7796_WriteData(y0 & 0xFF);
    ST7796_WriteData(y1 >> 8);
    ST7796_WriteData(y1 & 0xFF);
}

//整屏填充颜色
void ST7796_FillScreen(uint16_t color)
{
    static uint8_t buffer[512];

    // 256个RGB565像素
    for (uint16_t i = 0; i < 256; i++)
    {
        buffer[i * 2]     = color >> 8;
        buffer[i * 2 + 1] = color & 0xFF;
    }

    // 设置整屏显示区域
    ST7796_SetAddressWindow(
        0, 0,
        ST7796_WIDTH - 1,
        ST7796_HEIGHT - 1
    );

    // 确保板载Flash未被选中
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port,
                      FLASH_CS_Pin, GPIO_PIN_SET);

    // 选中LCD，准备发送RAMWR命令
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,
                      LCD_CS_Pin, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_DC_GPIO_Port,
                      LCD_DC_Pin, GPIO_PIN_RESET);

    uint8_t cmd = 0x2C;

    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);

    // 后面连续发送像素数据
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port,
                      LCD_DC_Pin, GPIO_PIN_SET);

    uint32_t remaining = ST7796_WIDTH * ST7796_HEIGHT;

    while (remaining > 0)
    {
        uint16_t count = (remaining > 256) ?
                          256 : (uint16_t)remaining;

        HAL_SPI_Transmit(
            &hspi1,
            buffer,
            count * 2,
            HAL_MAX_DELAY
        );

        remaining -= count;
    }

    // 发送结束，释放LCD片选
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,
                      LCD_CS_Pin, GPIO_PIN_SET);
}