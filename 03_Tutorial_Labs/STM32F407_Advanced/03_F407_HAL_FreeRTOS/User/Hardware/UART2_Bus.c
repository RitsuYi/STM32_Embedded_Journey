#include "UART2_Bus.h"
#include "usart.h"
#include "stream_buffer.h"
#include "task.h"
#include <string.h>

#define UART2_RX_DMA_BYTES    1024U
#define UART2_RX_STREAM_BYTES 1024U

/* DMA直接写普通SRAM中的固定数组，不能放到F407的CCM。
 * static让内存在整个程序运行期间有效，4字节对齐便于硬件访问。
 * 保留半满/写满中断：连续数据即使没有串口空闲间隔也能及时取走。 */
static uint8_t rx_dma[UART2_RX_DMA_BYTES] __attribute__((aligned(4)));

/* Stream Buffer是FreeRTOS提供的有界字节缓冲，会复制保存收到的数据。
 * 静态接口多留一个字节区分空/满，1025字节提供1024字节有效容量。
 * 不另加软件环形队列；此容量能容纳当前协议的一帧最大526字节数据。
 * 中断是唯一写入者，Camera_Task是唯一读取者。 */
static uint8_t rx_storage[UART2_RX_STREAM_BYTES + 1U];
static StaticStreamBuffer_t rx_control;
static StreamBufferHandle_t rx_stream;
static uint16_t rx_old_pos;
static volatile bool rx_ready; /* 中断遇到丢字节或UART错误时清零。 */
static bool stop_requested;    /* 异步停止只发起一次，之后让出CPU等硬件完成。 */
volatile uint32_t g_uart2_last_rx_error, g_uart2_rx_overflows;

/* DMA发送源必须一直有效。即使超时后DMA尚未停稳，也保留原数据。
 * JSON命令最坏转义后小于300字节；无需分配第二个526字节发送环形队列。 */
static uint8_t tx_dma[300] __attribute__((aligned(4)));
static bool tx_stop_requested;

bool UART2_Bus_Write(const uint8_t *data, uint16_t length, TickType_t timeout_ticks)
{
    if (!data || !length || length > sizeof(tx_dma) || !huart2.hdmatx) return false;
    if (huart2.gState != HAL_UART_STATE_READY ||
        HAL_DMA_GetState(huart2.hdmatx) != HAL_DMA_STATE_READY ||
        (huart2.hdmatx->Instance->CR & DMA_SxCR_EN) != 0U)
    {
        /* 上次发送/异常停止尚未结束，只请求一次停止，不覆盖原DMA源。 */
        if (!tx_stop_requested)
            tx_stop_requested = HAL_UART_AbortTransmit_IT(&huart2) == HAL_OK;
        return false;
    }
    tx_stop_requested = false;
    memcpy(tx_dma, data, length);
    if (HAL_UART_Transmit_DMA(&huart2, tx_dma, length) != HAL_OK)
    {
        tx_stop_requested = HAL_UART_AbortTransmit_IT(&huart2) == HAL_OK;
        return false;
    }
    TickType_t start = xTaskGetTickCount();
    while (huart2.gState != HAL_UART_STATE_READY)
    {
        if ((TickType_t)(xTaskGetTickCount() - start) >= timeout_ticks)
        {
            tx_stop_requested = HAL_UART_AbortTransmit_IT(&huart2) == HAL_OK;
            return false;
        }
        /* 不轮询忙等；发送期间GUI、按键、OLED及蜂鸣器可以获得CPU。 */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return huart2.ErrorCode == HAL_UART_ERROR_NONE;
}

bool UART2_Bus_Init(void)
{
    if (rx_ready) return true;
    if (huart2.hdmarx == NULL) return false;
    if (rx_stream == NULL)
    {
        rx_stream = xStreamBufferCreateStatic(sizeof(rx_storage), 1U,
                                              rx_storage, &rx_control);
        if (rx_stream == NULL) return false;
    }

    /* 只暂时关闭两个接收中断，避免清缓冲时又写入数据；不屏蔽整个系统。
     * IT接口只请求停止，不循环等待DMA：未停稳就返回false，由任务休眠。 */
    HAL_NVIC_DisableIRQ(USART2_IRQn);
    HAL_NVIC_DisableIRQ(DMA1_Stream5_IRQn);
    if (!stop_requested &&
        (huart2.RxState != HAL_UART_STATE_READY ||
         HAL_DMA_GetState(huart2.hdmarx) != HAL_DMA_STATE_READY ||
         (huart2.hdmarx->Instance->CR & DMA_SxCR_EN) != 0U))
    {
        stop_requested = true;
        (void)HAL_UART_AbortReceive_IT(&huart2);
        /* UART错误路径可能已关闭DMA请求位，却仍留下活动DMA流。
         * 仅在它仍为BUSY时补发异步停止，不重复打断已经在停止的DMA。 */
        if (HAL_DMA_GetState(huart2.hdmarx) == HAL_DMA_STATE_BUSY)
            (void)HAL_DMA_Abort_IT(huart2.hdmarx);
    }

    /* EN是DMA的运行开关；HAL状态也要就绪，防止旧停止回调干扰新接收。
     * 确认都已停止，才清除旧数据、重置写位置并重新使用同一块数组。 */
    if (huart2.RxState == HAL_UART_STATE_READY &&
        HAL_DMA_GetState(huart2.hdmarx) == HAL_DMA_STATE_READY &&
        (huart2.hdmarx->Instance->CR & DMA_SxCR_EN) == 0U &&
        xStreamBufferReset(rx_stream) == pdPASS)
    {
        HAL_NVIC_ClearPendingIRQ(USART2_IRQn);
        HAL_NVIC_ClearPendingIRQ(DMA1_Stream5_IRQn);
        rx_old_pos = 0U;
        stop_requested = false;
        rx_ready = true;

        /* 接收数组在整个传输期间保持有效，Circular模式只需启动一次。
         * 正常空闲/半满/写满回调不会停止或重新启动这个DMA。 */
        if (HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_dma,
                                        UART2_RX_DMA_BYTES) != HAL_OK)
            rx_ready = false;
    }

    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    return rx_ready;
}

/* 中断内只复制新增字节，空间不足立即标错，不等待缓冲腾空。
 * Read随后返回-1，让任务丢弃残帧并恢复，不能把断裂数据继续交给协议解析。 */
static void copy_from_isr(uint16_t offset, uint16_t length, BaseType_t *woken)
{
    if (length != 0U && rx_ready &&
        xStreamBufferSendFromISR(rx_stream, &rx_dma[offset], length, woken) != length)
    {
        ++g_uart2_rx_overflows; /* 缓冲满丢字节与线路噪声分开记录。 */
        rx_ready = false;
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart != &huart2 || !rx_ready) return;
    (void)Size;

    /* NDTR是DMA当前还剩多少字节未搬，由它计算实际写位置。
     * 事件可能排队，读取当前位置可避免迟到的半满事件带着旧Size造成重复复制。
     * USART2和DMA1 Stream5的抢占优先级都保持6，两个入口不能相互嵌套。 */
    uint16_t pos = (uint16_t)(UART2_RX_DMA_BYTES -
                              __HAL_DMA_GET_COUNTER(huart->hdmarx));
    if (pos == UART2_RX_DMA_BYTES) pos = 0U;
    BaseType_t woken = pdFALSE;

    if (pos > rx_old_pos)
        copy_from_isr(rx_old_pos, (uint16_t)(pos - rx_old_pos), &woken);
    else if (pos < rx_old_pos)
    {
        /* 写指针绕回开头时，按原接收顺序先取尾部，再取头部。 */
        copy_from_isr(rx_old_pos, (uint16_t)(UART2_RX_DMA_BYTES - rx_old_pos), &woken);
        copy_from_isr(0U, pos, &woken);
    }
    rx_old_pos = pos;
    portYIELD_FROM_ISR(woken);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2)
    {
        g_uart2_last_rx_error = huart->ErrorCode;
        rx_ready = false; /* 中断只标错，任务下次调用Init时恢复。 */
    }
}

int32_t UART2_Bus_Read(uint8_t *data, uint16_t capacity, TickType_t timeout_ticks)
{
    if (data == NULL || capacity == 0U || !rx_ready) return -1;

    size_t count = xStreamBufferReceive(rx_stream, data, capacity, timeout_ticks);
    /* 等待期间若发生错误，本批数据废弃，不能交给后续协议解析器。 */
    return rx_ready ? (int32_t)count : -1;
}
