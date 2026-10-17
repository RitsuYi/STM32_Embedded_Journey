#include "SPI1_Bus.h"
#include "task.h"  // 查询FreeRTOS调度器是否正在运行
#include "semphr.h"
#include "spi.h"

static StaticSemaphore_t mutex_storage;
static SemaphoreHandle_t mutex;
static StaticSemaphore_t tx_done_storage;
static SemaphoreHandle_t tx_done;

/* 中断写入本次结果，等待任务收到信号量后读取。
 * volatile要求编译器每次读取实际内存中的值。 */
static volatile HAL_StatusTypeDef tx_result = HAL_ERROR;

bool SPI1_Bus_Init(void)
{
    if (!mutex)
        mutex = xSemaphoreCreateRecursiveMutexStatic(&mutex_storage);

    if (!tx_done)
        tx_done = xSemaphoreCreateBinaryStatic(&tx_done_storage);

    return mutex != NULL && tx_done != NULL;
}
bool SPI1_Bus_Lock(void)
{
    return mutex && xSemaphoreTakeRecursive(mutex, portMAX_DELAY) == pdTRUE;
}
void SPI1_Bus_Unlock(void)
{
    if(mutex) (void)xSemaphoreGiveRecursive(mutex);
}

volatile uint32_t g_spi1_tx_done_count = 0;
volatile uint32_t g_spi1_error_count = 0;
volatile uint32_t g_spi1_last_error = 0;

static void notify_tx_done_from_isr(void)
{
    BaseType_t task_woken = pdFALSE;

    if (tx_done == NULL) return;

    (void)xSemaphoreGiveFromISR(tx_done, &task_woken);
    portYIELD_FROM_ISR(task_woken);
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    /* hspi指向产生回调的SPI；其他SPI不能修改SPI1的结果。 */
    if (hspi != &hspi1) return;

    ++g_spi1_tx_done_count;

    /* 先保存成功结果，再唤醒等待任务，保证任务读到正确状态。 */
    tx_result = HAL_OK;
    notify_tx_done_from_isr();
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    /* 必须先筛选SPI1，避免其他SPI的错误影响本次等待。 */
    if (hspi != &hspi1) return;

    /* 保存错误信息，方便之后在调试器中检查。 */
    g_spi1_last_error = HAL_SPI_GetError(hspi);
    ++g_spi1_error_count;

    /* 错误也要通知，让任务及时醒来执行异常收尾。 */
    tx_result = HAL_ERROR;
    notify_tx_done_from_isr();
}

HAL_StatusTypeDef SPI1_Bus_TransmitDMA(const uint8_t *data,
                                      uint16_t size,
                                      TickType_t timeout_ticks)
{
    /* 中断中不能休眠等待；调度器启动后，任务才能等待信号量。 */
    if (__get_IPSR() != 0U ||
        xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
        return HAL_ERROR;

    /* 拒绝无效参数，以及尚未完成初始化的情况。 */
    if (data == NULL || size == 0U || timeout_ticks == 0U ||
        tx_done == NULL || hspi1.hdmatx == NULL)
        return HAL_ERROR;

    /* 本接口依赖8位数据和Normal模式，即指定数量发送完后停止。 */
    if (hspi1.Init.DataSize != SPI_DATASIZE_8BIT ||
        hspi1.hdmatx->Init.Mode != DMA_NORMAL)
        return HAL_ERROR;

    /* 使用已有递归锁，允许调用者已经持有外层总线锁。 */
    if (!SPI1_Bus_Lock())
        return HAL_ERROR;

    HAL_StatusTypeDef status;

    /* 正在进行的操作不属于本次发送，不能擅自中止它。
     * DMA的EN位表示硬件传输通道是否仍处于启用状态。 */
    if (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY ||
        HAL_DMA_GetState(hspi1.hdmatx) != HAL_DMA_STATE_READY ||
        (hspi1.hdmatx->Instance->CR & DMA_SxCR_EN) != 0U ||
        __HAL_SPI_GET_FLAG(&hspi1, SPI_FLAG_BSY) != RESET)
    {
        status = HAL_BUSY;
        goto out;  // 集中到函数末尾释放本次取得的锁
    }

    /* 二值信号量最多保存一次通知；先取走上次可能遗留的通知。 */
    (void)xSemaphoreTake(tx_done, 0);
    tx_result = HAL_BUSY;

    status = HAL_SPI_Transmit_DMA(&hspi1, data, size);

    /* 返回BUSY表示未启动本次发送，不去中止其他操作。 */
    if (status == HAL_BUSY)
        goto out;

    if (status == HAL_OK)
    {
        /* 等待期间当前任务休眠，CPU可以运行其他任务。
         * 即使DMA先完成，信号量也会保留通知，不会漏掉结果。 */
        if (xSemaphoreTake(tx_done, timeout_ticks) == pdTRUE)
            status = (tx_result == HAL_OK) ? HAL_OK : HAL_ERROR;
        else
            status = HAL_TIMEOUT;
    }

    if (status != HAL_OK)
    {
        /* HAL中止操作可能清除错误码，因此先保存诊断信息。 */
        uint32_t error = HAL_SPI_GetError(&hspi1);
        if (error != HAL_SPI_ERROR_NONE)
            g_spi1_last_error = error;

        /* 启动失败、传输错误或超时，都必须先停止本次传输。 */
        if (HAL_SPI_Abort(&hspi1) != HAL_OK)
            Error_Handler();

        /* 部分错误路径已关闭SPI请求，但DMA通道可能仍启用，
         * 所以还要检查硬件EN位，必要时单独中止DMA。 */
        if ((hspi1.hdmatx->Instance->CR & DMA_SxCR_EN) != 0U)
        {
            if (HAL_DMA_Abort(hspi1.hdmatx) != HAL_OK)
                Error_Handler();
        }

        /* BSY清零表示SPI不再忙，之后才能安全结束片选事务。
         * 若始终无法停止，进入现有错误处理，避免返回后复用缓冲。 */
        uint32_t stop_started = HAL_GetTick();
        while (__HAL_SPI_GET_FLAG(&hspi1, SPI_FLAG_BSY) != RESET)
        {
            if ((HAL_GetTick() - stop_started) >= 10U)
                Error_Handler();
        }
    }

out:
    SPI1_Bus_Unlock();
    return status;
}
