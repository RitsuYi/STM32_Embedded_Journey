#ifndef FT6336U_H
#define FT6336U_H

#include <stdbool.h>
#include <stdint.h>

/* 七位地址0x38，传给STM32 HAL时由驱动左移一位成为0x70。
 * 本模组原生触摸范围按320×480设置，最终输出为横屏480×320。
 * MADCTL=0x28交换了液晶行列，因此也交换触摸X/Y。
 * 2026-10-17用户观察光标确认上下镜像：交换后还要把屏幕Y改为319-Y。
 * 这里只反转交换后的屏幕Y，不能误改原始Y或同时翻转屏幕X。
 * MIN/MAX是原始坐标端点；只有实测发现范围不符时才调整，不能随意猜标定值。 */
#define FT6336U_I2C_ADDRESS_7BIT  0x38U
#define FT6336U_RAW_X_MIN         0U
#define FT6336U_RAW_X_MAX         319U
#define FT6336U_RAW_Y_MIN         0U
#define FT6336U_RAW_Y_MAX         479U
#define FT6336U_SWAP_XY           1
#define FT6336U_INVERT_X          0
#define FT6336U_INVERT_Y          1 /* 修正上下相反：最终(x,y)=(raw_y,319-raw_x)。 */

typedef struct {
    uint16_t x, y; /* 已换算的屏幕像素位置；松手时保留最后一次有效位置。 */
    bool pressed;  /* true按下，false松手；本工程只向LVGL提供单指输入。 */
} FT6336U_Point;

/* 仅在GUI任务中调用：PE1拉低20ms后拉高，至少等300ms再通信。
 * 返回true表示芯片ID核对成功；保持商家硬复位后的默认模式，不额外写模式/扫描参数。
 * 返回false时保留显示/按键功能，Read会每隔1秒尝试重新通信。 */
bool FT6336U_Init(void);

/* point是接收结果的非空指针；返回true表示成功获取有效状态（包括松手）。
 * 返回false表示未就绪/通信失败/非法数据，同时point->pressed必定为false。
 * 按商家先读点数，再读第一个触点4字节；仅单指CONTACT有效，DOWN/UP和多指不按下。
 * 有效坐标只做方向转换，不滤波、不猜位置；原始双零记录按商家要求拒绝。
 * 由GUI任务定时调用，不能在中断或其他任务中同时调用。 */
bool FT6336U_Read(FT6336U_Point *point);

#endif
