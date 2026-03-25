#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"

#define OLED_WIDTH   128U
#define OLED_HEIGHT   64U

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowCharMode(uint8_t Line, uint8_t Column, char Char, uint8_t Inverse);
void OLED_ShowStringMode(uint8_t Line, uint8_t Column, char *String, uint8_t Inverse);
void OLED_ShowBigChar(uint8_t Page, uint8_t X, char Char);
void OLED_ShowBigString(uint8_t Page, uint8_t X, char *String);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ClearBuffer(void);
void OLED_Update(void);
void OLED_DrawPixel(int16_t X, int16_t Y, uint8_t Color);
void OLED_DrawLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, uint8_t Color);
void OLED_DrawRect(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, uint8_t Color);
void OLED_DrawFilledRect(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, uint8_t Color);
void OLED_DrawCircle(int16_t X0, int16_t Y0, uint8_t Radius, uint8_t Color);
uint16_t OLED_GetStringWidth(const char *String, uint8_t ScalePercent);
void OLED_DrawStringScaled(int16_t X, int16_t Y, const char *String, uint8_t ScalePercent, uint8_t Color);

#endif
