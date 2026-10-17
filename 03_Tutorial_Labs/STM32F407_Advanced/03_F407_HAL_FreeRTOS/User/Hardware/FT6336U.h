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

/* H6.5临时排障开关：1在已有OLED上显示触摸坐标/错误，0恢复原OLED内容。
 * 只供定位当前漏点问题，不能当成正式测量界面；不修改坐标方向或触摸灵敏度。 */
#define FT6336U_OLED_DIAGNOSTICS  0 /* 原因已查明，恢复OLED原显示；需要时可再开启。 */

typedef struct {
    uint16_t x, y; /* 已换算的屏幕像素位置；松手时保留最后一次有效位置。 */
    bool pressed;  /* true按下，false松手；本工程只向LVGL提供单指输入。 */
} FT6336U_Point;

/* 在调试器中展开g_ft6336u：ready只表示通信初始化成功，不代表实机位置已校准。
 * read_count是成功读出的数据帧数；error_count是通信/锁失败次数；
 * invalid_count是非法点数、事件或坐标次数；press_count是松手→按下次数。
 * last_status沿用HAL状态：0成功、1错误、2忙、3超时；last_i2c_error是HAL错误位。
 * int_level只观察PE2电平，不用它决定是否读取，以免漏掉松手。
 * out_of_range_count单独统计原始坐标越界，用来区别映射范围错误和通信失败；
 * read_gap_ms/max_read_gap_ms记录实际两次读取入口的间隔/最大间隔，单位毫秒。
 * last_event记录最近读到的事件高位：0按下、1抬起、2接触、3无效。 */
typedef struct {
    uint32_t ready, init_count, read_count, error_count, invalid_count, press_count;
    uint32_t last_status, last_i2c_error;
    uint32_t out_of_range_count, read_gap_ms, max_read_gap_ms;
    uint16_t raw_x, raw_y, x, y;
    uint8_t chip_id, firmware_id, vendor_id, point_count, pressed, int_level, last_event;
} FT6336U_Diagnostics;
extern volatile FT6336U_Diagnostics g_ft6336u;

/* 仅在GUI任务中调用：PE1拉低20ms后拉高，至少等300ms再通信。
 * 返回true表示地址应答、芯片ID及工作/轮询模式设置成功。
 * 返回false时保留显示/按键功能，Read会每隔1秒尝试重新通信。 */
bool FT6336U_Init(void);

/* point是接收结果的非空指针；返回true表示成功获取有效状态（包括松手）。
 * 返回false表示未就绪/通信失败/非法数据，同时point->pressed必定为false。
 * 两指同时按下时主动松手，避免将第二根手指误当作同一根手指拖动控件。
 * 由GUI任务定时调用，不能在中断或其他任务中同时调用。 */
bool FT6336U_Read(FT6336U_Point *point);

#endif
