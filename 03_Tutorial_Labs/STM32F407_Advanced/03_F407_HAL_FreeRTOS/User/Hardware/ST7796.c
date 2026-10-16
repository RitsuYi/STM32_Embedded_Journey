#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "spi.h"
#include "ST7796.h"
#include "SPI1_Bus.h"

void ST7796_WriteCommand(uint8_t cmd)
{
    if(!SPI1_Bus_Lock()) return;
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    SPI1_Bus_Unlock();
}

void ST7796_WriteData(uint8_t data)
{
    if(!SPI1_Bus_Lock()) return;
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
    HAL_SPI_Transmit(&hspi1, &data, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    SPI1_Bus_Unlock();
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
    if(!SPI1_Bus_Init()) return;
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

    // 当前ST7796 LCD模组经五色测试确认需要INVON
    ST7796_WriteCommand(0x21);

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
    if(!SPI1_Bus_Lock()) return;
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
    SPI1_Bus_Unlock();
}

//整屏填充颜色
void ST7796_FillScreen(uint16_t color)
{
    ST7796_FillRect(
        0, 0,
        ST7796_WIDTH,
        ST7796_HEIGHT,
        color
    );
}

/*
 * @brief 填充指定矩形区域
 * @param x      左上角X坐标
 * @param y      左上角Y坐标
 * @param width  矩形宽度
 * @param height 矩形高度
 * @param color  RGB565颜色
 */
void ST7796_FillRect(uint16_t x,
                     uint16_t y,
                     uint16_t width,
                     uint16_t height,
                     uint16_t color)
{
    static uint8_t buffer[512];

    // 1. 检查绘制区域是否合法
    if (width == 0 || height == 0 ||
        x >= ST7796_WIDTH ||
        y >= ST7796_HEIGHT)
    {
        return;
    }

    // 2. 防止矩形超出屏幕范围
    uint32_t xEnd = (uint32_t)x + width;
    uint32_t yEnd = (uint32_t)y + height;

    if (xEnd > ST7796_WIDTH)
        xEnd = ST7796_WIDTH;

    if (yEnd > ST7796_HEIGHT)
        yEnd = ST7796_HEIGHT;

    // 3. 计算实际需要发送的像素数量
    uint32_t remaining =
        (xEnd - x) * (yEnd - y);

    // 4. 准备256个RGB565像素
    for (uint16_t i = 0; i < 256; i++)
    {
        buffer[i * 2]     = color >> 8;
        buffer[i * 2 + 1] = color & 0xFF;
    }

    if(!SPI1_Bus_Lock()) return;

    // 5. 设置矩形显示区域
    ST7796_SetAddressWindow(
        x, y,
        (uint16_t)(xEnd - 1),
        (uint16_t)(yEnd - 1)
    );

    // 6. 确保板载Flash未被选中
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port,
                      FLASH_CS_Pin, GPIO_PIN_SET);

    // 7. 发送RAMWR命令
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,
                      LCD_CS_Pin, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_DC_GPIO_Port,
                      LCD_DC_Pin, GPIO_PIN_RESET);

    uint8_t cmd = 0x2C;

    HAL_SPI_Transmit(&hspi1, &cmd,
                     1, HAL_MAX_DELAY);

    // 8. 切换为数据模式
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port,
                      LCD_DC_Pin, GPIO_PIN_SET);

    // 9. 分块发送RGB565像素
    while (remaining > 0)
    {
        uint16_t count =
            (remaining > 256) ?
            256 : (uint16_t)remaining;

        HAL_SPI_Transmit(&hspi1,
                         buffer,
                         count * 2,
                         HAL_MAX_DELAY);

        remaining -= count;
    }

    // 10. 结束LCD通信
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,
                      LCD_CS_Pin, GPIO_PIN_SET);
    SPI1_Bus_Unlock();
}

HAL_StatusTypeDef ST7796_WriteRGB565(uint16_t x, uint16_t y,
                                    uint16_t width, uint16_t height,
                                    const uint8_t *pixels, uint32_t stride_bytes)
{
    static uint8_t tx[512]; /* Serialized by the SPI1 lock; no large task stack buffer. */
    uint32_t xe = (uint32_t)x + width, ye = (uint32_t)y + height;
    if(!pixels || !width || !height || xe > ST7796_WIDTH || ye > ST7796_HEIGHT ||
       stride_bytes < (uint32_t)width * 2U) return HAL_ERROR;
    if(!SPI1_Bus_Lock()) return HAL_ERROR;
    uint16_t x1 = (uint16_t)(xe - 1U), y1 = (uint16_t)(ye - 1U);
    uint8_t commands[] = {0x2A, 0x2B, 0x2C};
    uint8_t params[2][4] = {
        {(uint8_t)(x >> 8), (uint8_t)x, (uint8_t)(x1 >> 8), (uint8_t)x1},
        {(uint8_t)(y >> 8), (uint8_t)y, (uint8_t)(y1 >> 8), (uint8_t)y1}
    };
    HAL_StatusTypeDef status = HAL_OK;
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    for(unsigned i = 0; i < 3; ++i) {
        HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);
        status = HAL_SPI_Transmit(&hspi1, &commands[i], 1, HAL_MAX_DELAY);
        if(status != HAL_OK) goto done;
        HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
        if(i < 2) {
            status = HAL_SPI_Transmit(&hspi1, params[i], 4, HAL_MAX_DELAY);
            if(status != HAL_OK) goto done;
        }
    }
    for(uint16_t row = 0; row < height; ++row) {
        const uint8_t *src = pixels + (uint32_t)row * stride_bytes;
        uint16_t remaining = width;
        while(remaining) {
            uint16_t count = remaining > 256U ? 256U : remaining;
            for(uint16_t i = 0; i < count; ++i) {
                tx[2U*i] = src[2U*i+1U];
                tx[2U*i+1U] = src[2U*i];
            }
            status = HAL_SPI_Transmit(&hspi1, tx, (uint16_t)(count * 2U), HAL_MAX_DELAY);
            if(status != HAL_OK) goto done;
            src += count * 2U;
            remaining -= count;
        }
    }
done:
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    SPI1_Bus_Unlock();
    return status;
}
