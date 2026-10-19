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
/* 只有持锁任务能收尾；ISR只写结果和通知，绝不归还任务互斥锁。 */
static volatile bool tx_active;
static TaskHandle_t tx_owner;
static TickType_t tx_started;

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
    if (hspi != &hspi1 || !tx_active) return;

    /* 先保存成功结果，再唤醒等待任务，保证任务读到正确状态。 */
    tx_result = HAL_OK;
    notify_tx_done_from_isr();
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    /* 必须先筛选SPI1，避免其他SPI的错误影响本次等待。 */
    if (hspi != &hspi1 || !tx_active) return;

    /* 错误也要通知，让任务及时醒来执行异常收尾。 */
    tx_result = HAL_ERROR;
    notify_tx_done_from_isr();
}

bool SPI1_Bus_DMAAddressValid(uintptr_t address, uint32_t bytes)
{
    /* F407普通SRAM为0x20000000..0x2001FFFF，片内Flash为0x08000000..0x080FFFFF。
     * 先减上界再比较长度，避免address+bytes整数溢出；0x10000000的CCM不在其中。 */
    return bytes && ((address >= 0x20000000U && address < 0x20020000U &&
                      bytes <= 0x20020000U-address) ||
                     (address >= 0x08000000U && address < 0x08100000U &&
                      bytes <= 0x08100000U-address));
}

/* HAL在少数错误分支中已关SPI请求，但DMA EN还可能为1，因此分别检查并终止。
 * 若硬件无法停止，保留原停机保护，不允许调用者提前释放DMA仍在读取的缓冲。 */
static void stop_dma(void)
{
    if(HAL_SPI_Abort(&hspi1) != HAL_OK) Error_Handler();
    if(hspi1.hdmatx && ((hspi1.hdmatx->Instance->CR & DMA_SxCR_EN) != 0U ||
       HAL_DMA_GetState(hspi1.hdmatx)!=HAL_DMA_STATE_READY) &&
       HAL_DMA_Abort(hspi1.hdmatx) != HAL_OK) Error_Handler();
    uint32_t started=HAL_GetTick();
    while(__HAL_SPI_GET_FLAG(&hspi1,SPI_FLAG_BSY) != RESET) {
        if(HAL_GetTick()-started >= 10U) Error_Handler();
    }
    if(hspi1.hdmatx && (hspi1.hdmatx->Instance->CR & DMA_SxCR_EN) != 0U) Error_Handler();
}

bool SPI1_Bus_DMAOwnedByCaller(void)
{
    return __get_IPSR()==0U && xTaskGetSchedulerState()==taskSCHEDULER_RUNNING &&
           tx_active && tx_owner==xTaskGetCurrentTaskHandle();
}
void SPI1_Bus_AbortPolling(void)
{
    /* 仅供已经持锁的LCD命令阶段；不能用此函数越过另一个在途DMA的所有权。 */
    if(__get_IPSR()!=0U || xTaskGetSchedulerState()!=taskSCHEDULER_RUNNING || tx_active)
        Error_Handler();
    stop_dma();
}

HAL_StatusTypeDef SPI1_Bus_StartDMA(const uint8_t *data, uint16_t size)
{
    /* 中断中不能休眠等待；调度器启动后，任务才能等待信号量。 */
    if (__get_IPSR() != 0U ||
        xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
        return HAL_ERROR;

    /* 拒绝无效参数，以及尚未完成初始化的情况。 */
    if (data == NULL || size == 0U ||
        tx_done == NULL || hspi1.hdmatx == NULL)
        return HAL_ERROR;
#if defined(STM32F407xx)
    /* 主机替身地址与MCU不同；硬件构建才验证真实源地址，防止把CCM交给DMA。 */
    if(!SPI1_Bus_DMAAddressValid((uintptr_t)data,size)) return HAL_ERROR;
#endif

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
    if (tx_active || HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY ||
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
    tx_owner=xTaskGetCurrentTaskHandle();
    tx_started=xTaskGetTickCount();
    tx_active=true; /* 在启动前发布，DMA即使立即完成也不会漏掉回调。 */
    __DMB(); /* F407无数据缓存；内存屏障仍确保像素写入先于DMA硬件启动。 */

    status = HAL_SPI_Transmit_DMA(&hspi1, data, size);

    /* 返回BUSY表示未启动本次发送，不去中止其他操作。 */
    if(status == HAL_OK) return HAL_OK; /* 故意保留递归锁，交给同一任务的Wait释放。 */
    if(status != HAL_BUSY) stop_dma();
    tx_active=false;tx_owner=NULL;

out:
    SPI1_Bus_Unlock();
    return status;
}

HAL_StatusTypeDef SPI1_Bus_WaitDMA(TickType_t timeout_ticks)
{
    if(!SPI1_Bus_DMAOwnedByCaller()) return HAL_ERROR;
    /* 总等待期限从Start开始算，包含CPU绘制下一条带的时间；先消费已到达的通知。
     * timeout=0仍允许收尾已经完成的传输，否则安全中止，不遗留总线锁。 */
    BaseType_t done=xSemaphoreTake(tx_done,0);
    TickType_t elapsed=xTaskGetTickCount()-tx_started;
    if(done!=pdTRUE && elapsed<timeout_ticks)
        done=xSemaphoreTake(tx_done,timeout_ticks-elapsed);
    HAL_StatusTypeDef status=done==pdTRUE ? tx_result : HAL_TIMEOUT;
    /* HAL Normal TX完成回调在TXE/BSY检查之后触发；额外核硬件，避免假完成导致早放缓冲。 */
    if(status==HAL_OK && ((hspi1.hdmatx->Instance->CR&DMA_SxCR_EN)!=0U ||
       __HAL_SPI_GET_FLAG(&hspi1,SPI_FLAG_BSY)!=RESET ||
       HAL_SPI_GetState(&hspi1)!=HAL_SPI_STATE_READY ||
       HAL_DMA_GetState(hspi1.hdmatx)!=HAL_DMA_STATE_READY)) status=HAL_ERROR;
    if(status!=HAL_OK) stop_dma();
    tx_active=false;tx_owner=NULL;
    (void)xSemaphoreTake(tx_done,0); /* 异常中止期间的通知不能污染下一次传输。 */
    SPI1_Bus_Unlock();
    return status;
}

HAL_StatusTypeDef SPI1_Bus_TransmitDMA(const uint8_t *data,uint16_t size,TickType_t timeout_ticks)
{
    /* 兼容原同步调用：保留源数据不变、完整等待和错误安全收尾的既有契约。 */
    if(timeout_ticks==0U) return HAL_ERROR;
    HAL_StatusTypeDef status=SPI1_Bus_StartDMA(data,size);
    return status==HAL_OK ? SPI1_Bus_WaitDMA(timeout_ticks) : status;
}
