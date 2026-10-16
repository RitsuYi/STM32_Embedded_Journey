#ifndef __OLED_H
#define __OLED_H

#include <stdint.h>

#include "OLED_Data.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SSD1306 0.96-inch 128x64 panel configuration. */
#define OLED_WIDTH                 128U
#define OLED_HEIGHT                64U
#define OLED_PAGE_COUNT            (OLED_HEIGHT / 8U)

/*
 * HAL expects the 7-bit address shifted left by one bit.
 * SSD1306 SA0 = 0: 0x3C << 1 = 0x78.
 */
#define OLED_I2C_ADDRESS           (0x3CU << 1)
#define OLED_I2C_TIMEOUT_MS        100U
#define OLED_POWER_ON_DELAY_MS     100U
#define OLED_COLUMN_OFFSET         0U

#define OLED_8X16                  8U
#define OLED_6X8                   6U

#define OLED_UNFILLED              0U
#define OLED_FILLED                1U

void OLED_Init(void);
uint8_t OLED_IsConnected(void);
uint32_t OLED_GetI2CLastError(void);
uint32_t OLED_GetI2CErrorCount(void);

void OLED_Update(void);
void OLED_UpdateArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);

void OLED_Clear(void);
void OLED_ClearArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);
void OLED_Reverse(void);
void OLED_ReverseArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);

void OLED_ShowChar(int16_t X, int16_t Y, char Char, uint8_t FontSize);
void OLED_ShowString(int16_t X, int16_t Y, char *String, uint8_t FontSize);
void OLED_ShowNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowSignedNum(int16_t X, int16_t Y, int32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowHexNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowBinNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowFloatNum(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize);
void OLED_ShowImage(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image);
void OLED_Printf(int16_t X, int16_t Y, uint8_t FontSize, char *format, ...);

void OLED_DrawPoint(int16_t X, int16_t Y);
uint8_t OLED_GetPoint(int16_t X, int16_t Y);
void OLED_DrawLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1);
void OLED_DrawRectangle(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, uint8_t IsFilled);
void OLED_DrawTriangle(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1,
                       int16_t X2, int16_t Y2, uint8_t IsFilled);
void OLED_DrawCircle(int16_t X, int16_t Y, uint8_t Radius, uint8_t IsFilled);
void OLED_DrawEllipse(int16_t X, int16_t Y, uint8_t A, uint8_t B, uint8_t IsFilled);
void OLED_DrawArc(int16_t X, int16_t Y, uint8_t Radius,
                  int16_t StartAngle, int16_t EndAngle, uint8_t IsFilled);

#ifdef __cplusplus
}
#endif

#endif
