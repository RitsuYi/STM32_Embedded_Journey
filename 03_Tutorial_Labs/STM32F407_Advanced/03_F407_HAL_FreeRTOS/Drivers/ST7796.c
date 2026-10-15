#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "spi.h"

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
