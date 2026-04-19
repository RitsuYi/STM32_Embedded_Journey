#ifndef __OLED_H
#define __OLED_H

#include "stm32f4xx.h"
#include <stdint.h>
#include "OLED_Data.h"

/* OLED hardware options */
#ifndef OLED_SCL_PORT
#define OLED_SCL_PORT            GPIOB
#endif

#ifndef OLED_SCL_PIN
#define OLED_SCL_PIN             GPIO_Pin_8
#endif

#ifndef OLED_SDA_PORT
#define OLED_SDA_PORT            GPIOB
#endif

#ifndef OLED_SDA_PIN
#define OLED_SDA_PIN             GPIO_Pin_9
#endif

/* PB8/PB9 default to I2C1 on STM32F407. */
#ifndef OLED_SCL_PINSOURCE
#define OLED_SCL_PINSOURCE       GPIO_PinSource8
#endif

#ifndef OLED_SDA_PINSOURCE
#define OLED_SDA_PINSOURCE       GPIO_PinSource9
#endif

/* StdPeriph expects the 8-bit write address, e.g. SSD1306 uses 0x78. */
#ifndef OLED_I2C_ADDRESS
#define OLED_I2C_ADDRESS         0x78
#endif

/* SH1106 1.3-inch modules often need a column offset of 2. */
#ifndef OLED_COLUMN_OFFSET
#define OLED_COLUMN_OFFSET       0
#endif

/* Give the panel time to power up before sending init commands. */
#ifndef OLED_POWER_ON_DELAY_MS
#define OLED_POWER_ON_DELAY_MS   100
#endif

/* Hardware I2C options */
#ifndef OLED_I2C_INSTANCE
#define OLED_I2C_INSTANCE        I2C1
#endif

#ifndef OLED_I2C_GPIO_AF
#define OLED_I2C_GPIO_AF         GPIO_AF_I2C1
#endif

#ifndef OLED_I2C_RCC_APB1_PERIPH
#define OLED_I2C_RCC_APB1_PERIPH RCC_APB1Periph_I2C1
#endif

#ifndef OLED_I2C_CLOCK_SPEED
#define OLED_I2C_CLOCK_SPEED     400000
#endif

#ifndef OLED_I2C_TIMEOUT
#define OLED_I2C_TIMEOUT         0xFFFF
#endif

#define OLED_I2C_ERROR_BUSY_TIMEOUT  0x40000000UL
#define OLED_I2C_ERROR_TIMEOUT       0x80000000UL

/* Public constants */
#define OLED_8X16                8
#define OLED_6X8                 6

#define OLED_UNFILLED            0
#define OLED_FILLED              1

/* Basic control */
void OLED_Init(void);
uint32_t OLED_GetI2CLastError(void);
uint32_t OLED_GetI2CErrorCount(void);

/* Refresh */
void OLED_Update(void);
void OLED_UpdateArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);

/* Framebuffer control */
void OLED_Clear(void);
void OLED_ClearArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);
void OLED_Reverse(void);
void OLED_ReverseArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);

/* Text and image */
void OLED_ShowChar(int16_t X, int16_t Y, char Char, uint8_t FontSize);
void OLED_ShowString(int16_t X, int16_t Y, char *String, uint8_t FontSize);
void OLED_ShowNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowSignedNum(int16_t X, int16_t Y, int32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowHexNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowBinNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowFloatNum(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize);
void OLED_ShowImage(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image);
void OLED_Printf(int16_t X, int16_t Y, uint8_t FontSize, char *format, ...);

/* Drawing */
void OLED_DrawPoint(int16_t X, int16_t Y);
uint8_t OLED_GetPoint(int16_t X, int16_t Y);
void OLED_DrawLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1);
void OLED_DrawRectangle(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, uint8_t IsFilled);
void OLED_DrawTriangle(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, uint8_t IsFilled);
void OLED_DrawCircle(int16_t X, int16_t Y, uint8_t Radius, uint8_t IsFilled);
void OLED_DrawEllipse(int16_t X, int16_t Y, uint8_t A, uint8_t B, uint8_t IsFilled);
void OLED_DrawArc(int16_t X, int16_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle, uint8_t IsFilled);

#endif
