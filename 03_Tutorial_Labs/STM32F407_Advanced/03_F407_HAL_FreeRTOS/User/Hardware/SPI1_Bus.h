#ifndef SPI1_BUS_H
#define SPI1_BUS_H
#include <stdbool.h>
#include "main.h"  // 提供 HAL_StatusTypeDef 等HAL类型
#include "FreeRTOS.h"       // 提供系统时钟节拍类型 TickType_t

/* All future SPI1 clients, including Flash, must hold this lock from CS low
 * through transfer completion and CS high. Call Init from the GUI task. */
bool SPI1_Bus_Init(void);
bool SPI1_Bus_Lock(void);
void SPI1_Bus_Unlock(void);

/* 异步分两阶段：Start成功后当前任务继续绘制另一块缓冲；Wait才等待并释放内部锁。
 * 调用者的外层锁仍覆盖CS低到CS高。两函数必须由同一运行中任务成对调用。
 * data/size为连续的LCD字节流，成功启动后直到Wait返回前禁止改写或释放data。 */
HAL_StatusTypeDef SPI1_Bus_StartDMA(const uint8_t *data, uint16_t size);
HAL_StatusTypeDef SPI1_Bus_WaitDMA(TickType_t timeout_ticks);
/* 仅做地址范围判断，拒绝CCM、栈外溢和跨区；真实固件Start也会执行此检查。 */
bool SPI1_Bus_DMAAddressValid(uintptr_t address, uint32_t bytes);
/* 查询收尾上下文，防止ISR/其他任务提前释放LCD片选；不等待、不修改状态。 */
bool SPI1_Bus_DMAOwnedByCaller(void);
/* 窗口命令轮询失败时使用：调用者已持总线锁且没有启动DMA，安全停止SPI后才拉高CS。 */
void SPI1_Bus_AbortPolling(void);

/*
 * 通过DMA发送一段数据，并让当前任务等待结果。
 *
 * data：待发送数据；必须位于DMA能访问的内存，不能放在CCM。
 * size：字节数，本接口用于当前8位SPI配置。
 * timeout_ticks：等待结果的最长系统节拍数，例如 pdMS_TO_TICKS(100)。
 *
 * 返回：HAL_OK成功、HAL_BUSY忙、HAL_TIMEOUT等待超时、HAL_ERROR出错。
 * 等待时间不包括获取总线锁和异常收尾的时间。
 *
 * 调用者仍须用外层总线锁保护整个片选事务：
 * 先加锁、选中设备，调用本函数，拉高CS后再解锁。
 */
HAL_StatusTypeDef SPI1_Bus_TransmitDMA(const uint8_t *data,
                                      uint16_t size,
                                      TickType_t timeout_ticks);


#endif
