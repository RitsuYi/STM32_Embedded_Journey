#ifndef UART2_BUS_H
#define UART2_BUS_H

#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"

/* 调试器可观察的两个累计故障线索，正常收发不需要查询。
 * last_rx_error保留最近一次HAL接收错误位：2噪声、4帧格式、8溢出、16 DMA；
 * rx_overflows为软件字节缓冲放不下的次数，与UART硬件溢出是两回事。
 * 重启接收不清零，避免错误在调试器读取前已被HAL恢复过程擦除。 */
extern volatile uint32_t g_uart2_last_rx_error, g_uart2_rx_overflows;

/* USART2只由Camera_Task操作，沿用CubeMX配置，不加任务锁或身份核验。
 * 在调度器启动后调用：首次创建固定缓冲并启动接收，正常时直接返回true。
 * 出错后异步停止旧接收；尚未停稳或启动失败返回false，任务应休眠后再试。
 * true仅表示本地接收已启动，不表示相机在线。 */
bool UART2_Bus_Init(void);

/* 仅由Camera_Task读取，最多复制capacity字节到data，不补字符串结束符。
 * timeout_ticks是最长休眠节拍数，用pdMS_TO_TICKS把毫秒换算成节拍。
 * 返回正数为收到的字节数，0为等待到期，-1为参数错误或接收已中断。
 * 收到-1后丢弃协议残帧，再调用Init恢复，避免把丢过字节的数据拼成假帧。 */
int32_t UART2_Bus_Read(uint8_t *data, uint16_t capacity, TickType_t timeout_ticks);

/* 发送先复制到模块的固定缓冲，再启动DMA并让任务休眠等完成。
 * 最长等待timeout_ticks；超时异步停止，缓冲在停止完成前不会被覆盖。
 * 仅Camera_Task调用，最多300字节；返回true只代表本地发送完成。 */
bool UART2_Bus_Write(const uint8_t *data, uint16_t length, TickType_t timeout_ticks);

#endif
