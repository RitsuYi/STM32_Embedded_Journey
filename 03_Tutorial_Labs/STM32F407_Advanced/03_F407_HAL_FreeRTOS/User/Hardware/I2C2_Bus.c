#include "I2C2_Bus.h"
#include "main.h"
#include "task.h"
#include "semphr.h"

/* 静态创建表示锁本身占固定内存，不额外申请FreeRTOS堆。
 * 必须在main完成OLED等HAL启动操作后、启动任务前创建。
 * 不提前创建，避免本工程FreeRTOS端口在调度前屏蔽TIM6，卡住HAL延时。 */
static StaticSemaphore_t mutex_storage;
static SemaphoreHandle_t mutex;

bool I2C2_Bus_Init(void)
{
    if (!mutex) mutex = xSemaphoreCreateMutexStatic(&mutex_storage);
    return mutex != NULL;
}

bool I2C2_Bus_Lock(TickType_t timeout_ticks)
{
    if (__get_IPSR() != 0U) return false;

    BaseType_t state = xTaskGetSchedulerState();
    /* 此时仅有main，允许锁尚未创建；不调用任何创建/取得信号量的接口，
     * 保持TIM6中断可用，让OLED_Init里的HAL_Delay和HAL通信超时正常计时。 */
    if (state == taskSCHEDULER_NOT_STARTED) return true;
    /* 任务期必须有锁；调度器暂停时也不能等待其他任务。 */
    if (state != taskSCHEDULER_RUNNING || !mutex) return false;
    return xSemaphoreTake(mutex, timeout_ticks) == pdTRUE;
}

void I2C2_Bus_Unlock(void)
{
    if (__get_IPSR() == 0U && mutex &&
        xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        (void)xSemaphoreGive(mutex);
}
