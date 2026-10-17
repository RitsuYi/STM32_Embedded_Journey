#ifndef I2C2_BUS_H
#define I2C2_BUS_H

#include <stdbool.h>
#include "FreeRTOS.h"

/* 在main完成OLED等所有依赖HAL计时的启动操作后、启动任务前调用一次。
 * 创建OLED和触摸共同使用的静态互斥锁，相当于总线“使用权”；失败返回false。
 * 本工程调度前创建RTOS对象会保持中断屏蔽，不能再跟依赖TIM6的HAL延时。 */
bool I2C2_Bus_Init(void);

/* 仅供普通任务使用：最多等待timeout_ticks个系统节拍，取得使用权返回true。
 * main在调度器启动前即使尚未Init也能使用，此时没有任务竞争，不调用RTOS锁接口。
 * 调度器启动后必须已Init，未建锁则返回false，确保不会跳过任务间互斥。
 * 每次成功取得使用权后，必须在HAL传输及错误码读取结束后调用Unlock。
 * 这是普通锁，不允许同一个任务连续加两次锁；中断中禁止调用。 */
bool I2C2_Bus_Lock(TickType_t timeout_ticks);
void I2C2_Bus_Unlock(void);

#endif
