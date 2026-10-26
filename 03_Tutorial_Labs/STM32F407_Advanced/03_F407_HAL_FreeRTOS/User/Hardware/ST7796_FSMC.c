#include "ST7796.h"

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdbool.h>
#include <string.h>

/* FSMC把外部LCD映射成CPU可写的地址，硬件自动产生CS、WR及RS时序。
 * NE1起始地址为0x60000000；16位总线使外部A16对应CPU地址位17，
 * 所以RS=1的数据地址必须加0x20000，不能写成0x60010000。
 * 命令/参数只使用DB7..DB0；RGB565像素一次送完整DB15..DB0。 */
#define LCD_COMMAND_ADDRESS 0x60000000U
#define LCD_DATA_ADDRESS    0x60020000U

/* volatile要求每个像素真正产生一次16位写操作，不能被编译器合并/省略。
 * 数据地址始终固定：LCD内部自动移动显存位置，CPU不能递增目标地址。 */
#ifndef ST7796_FSMC_WRITE16
#define ST7796_FSMC_WRITE16(address, value) \
    (*(volatile uint16_t *)(uintptr_t)(address) = (uint16_t)(value))
#endif

/* DMA（硬件自动搬运数据）独占DMA2 Stream0，USART2仍使用DMA1 Stream5/6。
 * 两块绘图缓冲由LVGL轮换；这里每次只发送其中一块，不启用DMA硬件DBM。
 * F407的内存到内存模式不支持DBM/循环模式，必须使用普通单次传输。 */
#define LCD_DMA_TIMEOUT_MS 100U
#define LCD_DMA_MAX_PIXELS 65535U
static DMA_HandleTypeDef lcd_dma;
static StaticSemaphore_t dma_done_storage;
static SemaphoreHandle_t dma_done;
static TaskHandle_t dma_owner;
static bool dma_initialized;
static volatile bool dma_active, dma_complete;
static volatile HAL_StatusTypeDef dma_result;
static uint32_t dma_started_ms;
/* 任务先填写参数，随后中断推进分段/换行；等待完成前任务不改这些参数。
 * row_remaining以“像素”为单位；stride以“字节”为单位，避免两者混用。 */
static const uint8_t *dma_source;
static uint32_t dma_row_pixels, dma_row_remaining, dma_rows, dma_stride, dma_chunk;

static void dma_signal(HAL_StatusTypeDef result)
{
    BaseType_t higher_priority_woken = pdFALSE;
    dma_result = result;
    __DMB();                       /* 先保存结果，再让被唤醒的GUI任务读取。 */
    dma_complete = true;
    (void)xSemaphoreGiveFromISR(dma_done, &higher_priority_woken);
    portYIELD_FROM_ISR(higher_priority_woken);
}

static HAL_StatusTypeDef dma_start_chunk(void)
{
    dma_chunk = dma_row_remaining > LCD_DMA_MAX_PIXELS ? LCD_DMA_MAX_PIXELS : dma_row_remaining;
    /* HAL的M2M（内存到内存）模式把源放PAR、目标放M0AR，因此：
     * PINC递增读取SRAM，MINC关闭使LCD目标地址固定；长度为16位像素个数。
     * FIFO（硬件内部小队列）用于M2M；单次半字访问保留已验收的FSMC写时序。 */
    /* 先清旧标志再开FIFO错误中断，避免新事务收到上一段遗留的IRQ。 */
    __HAL_DMA_CLEAR_FLAG(&lcd_dma, DMA_FLAG_FEIF0_4 | DMA_FLAG_DMEIF0_4 |
                         DMA_FLAG_TEIF0_4 | DMA_FLAG_HTIF0_4 | DMA_FLAG_TCIF0_4);
    __HAL_DMA_ENABLE_IT(&lcd_dma, DMA_IT_FE);
    __DMB();                       /* CPU画完的像素必须先写回普通SRAM，再启动DMA读取。 */
    return HAL_DMA_Start_IT(&lcd_dma, (uint32_t)(uintptr_t)dma_source,
                            LCD_DATA_ADDRESS, dma_chunk);
}

static void dma_error(DMA_HandleTypeDef *handle)
{
    (void)handle;
    /* 错误中断只通知任务，不在这里调用LVGL或带延时的HAL中止函数。 */
    if(dma_active && !dma_complete) dma_signal(HAL_ERROR);
}

static void dma_completed(DMA_HandleTypeDef *handle)
{
    if(!dma_active || dma_complete) return;
    /* 同一次IRQ可能既有完成又有错误，错误优先，不能继续发送下一行。 */
    if(handle->ErrorCode != HAL_DMA_ERROR_NONE) { dma_error(handle); return; }
    dma_source += dma_chunk * 2U;
    dma_row_remaining -= dma_chunk;
    if(dma_row_remaining == 0U) {
        --dma_rows;
        if(dma_rows == 0U) { dma_signal(HAL_OK); return; }
        /* 有行尾填充时只跳过padding，不把它当成屏幕像素发送。 */
        dma_source += dma_stride - dma_row_pixels * 2U;
        dma_row_remaining = dma_row_pixels;
    }
    if(dma_start_chunk() != HAL_OK) dma_signal(HAL_ERROR);
}

HAL_StatusTypeDef ST7796_DMAInit(void)
{
    if(dma_initialized) return HAL_OK;
    /* 由调度器运行后的唯一GUI任务调用，避免启动前创建RTOS对象影响时基。 */
    if(!dma_done) dma_done = xSemaphoreCreateBinaryStatic(&dma_done_storage);
    if(!dma_done) return HAL_ERROR;
    __HAL_RCC_DMA2_CLK_ENABLE();
    lcd_dma.Instance = DMA2_Stream0;
    lcd_dma.Init.Channel = DMA_CHANNEL_0;
    lcd_dma.Init.Direction = DMA_MEMORY_TO_MEMORY;
    lcd_dma.Init.PeriphInc = DMA_PINC_ENABLE;   /* M2M的PAR是源地址。 */
    lcd_dma.Init.MemInc = DMA_MINC_DISABLE;     /* M0AR固定为LCD数据寄存器。 */
    lcd_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    lcd_dma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    lcd_dma.Init.Mode = DMA_NORMAL;
    lcd_dma.Init.Priority = DMA_PRIORITY_LOW;
    lcd_dma.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    lcd_dma.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    lcd_dma.Init.MemBurst = DMA_MBURST_SINGLE;
    lcd_dma.Init.PeriphBurst = DMA_PBURST_SINGLE;
    if(HAL_DMA_Init(&lcd_dma) != HAL_OK) return HAL_ERROR;
    lcd_dma.XferCpltCallback = dma_completed;
    lcd_dma.XferErrorCallback = dma_error;
    /* 数值6不高于FreeRTOS允许调用FromISR接口的边界5，沿用串口DMA的规则。 */
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 6, 0);
    HAL_NVIC_ClearPendingIRQ(DMA2_Stream0_IRQn);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    dma_initialized = true;
    return HAL_OK;
}

void ST7796_DMAIRQHandler(void)
{
    HAL_DMA_IRQHandler(&lcd_dma);
}

static void dma_stop(void)
{
    /* 先屏蔽该流的IRQ，避免中止时完成回调又启动下一行；不影响串口DMA。 */
    HAL_NVIC_DisableIRQ(DMA2_Stream0_IRQn);
    if(lcd_dma.State == HAL_DMA_STATE_BUSY) (void)HAL_DMA_Abort(&lcd_dma);
    __HAL_DMA_DISABLE_IT(&lcd_dma, DMA_IT_TC | DMA_IT_HT | DMA_IT_TE | DMA_IT_DME);
    __HAL_DMA_DISABLE_IT(&lcd_dma, DMA_IT_FE);
    __HAL_DMA_DISABLE(&lcd_dma);
    uint32_t began = HAL_GetTick();
    while((lcd_dma.Instance->CR & DMA_SxCR_EN) != 0U) {
        /* 即使HAL报告错误，也必须核对硬件EN确实归零。
         * 无法停止时停机保护，绝不把仍被DMA读取的缓冲归还给LVGL。 */
        if(HAL_GetTick() - began >= 5U) { Error_Handler(); for(;;) { } }
    }
    __DSB();
    __HAL_DMA_CLEAR_FLAG(&lcd_dma, DMA_FLAG_FEIF0_4 | DMA_FLAG_DMEIF0_4 |
                         DMA_FLAG_TEIF0_4 | DMA_FLAG_HTIF0_4 | DMA_FLAG_TCIF0_4);
    HAL_NVIC_ClearPendingIRQ(DMA2_Stream0_IRQn);
    /* FIFO/传输错误可能留下HAL的BUSY/锁状态，安全停流后重新初始化才能重试。 */
    dma_initialized = HAL_DMA_Init(&lcd_dma) == HAL_OK;
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

HAL_StatusTypeDef ST7796_WriteRGB565Start(uint16_t x, uint16_t y,
                                        uint16_t width, uint16_t height,
                                        const uint8_t *pixels, uint32_t stride_bytes)
{
    /* 已有传输时直接拒绝，不能误停正在使用另一块缓冲的DMA。 */
    if(dma_active) return HAL_BUSY;
    uint32_t row_bytes = (uint32_t)width * 2U;
    uint64_t span = height ? (uint64_t)(height - 1U) * stride_bytes + row_bytes : 0U;
    uintptr_t address = (uintptr_t)pixels;
    if(!pixels || !width || !height || (uint32_t)x + width > ST7796_WIDTH ||
       (uint32_t)y + height > ST7796_HEIGHT || stride_bytes < row_bytes ||
       (address & 1U) || (height > 1U && (stride_bytes & 1U)) ||
       address < SRAM_BASE || address >= SRAM_BASE + 128U * 1024U ||
       span > SRAM_BASE + 128U * 1024U - address) return HAL_ERROR;
    /* 只接受F407普通SRAM内的半字对齐源，明确拒绝CCM及跨边界数据。
     * 调用者仍须保证源对象实际容量足够；地址合法不能证明对象没有越界。 */
    if(ST7796_DMAInit() != HAL_OK) return HAL_ERROR;
    while(xSemaphoreTake(dma_done, 0) == pdTRUE) { } /* 清除上次事务的遗留通知。 */
    dma_owner = xTaskGetCurrentTaskHandle();
    dma_source = pixels;
    dma_stride = stride_bytes;
    /* 紧密排列时整块发一次；有padding才逐行发。超65535像素由中断分段续传。 */
    dma_row_pixels = stride_bytes == row_bytes ? (uint32_t)width * height : width;
    dma_row_remaining = dma_row_pixels;
    dma_rows = stride_bytes == row_bytes ? 1U : height;
    dma_complete = false;
    dma_result = HAL_BUSY;
    dma_started_ms = HAL_GetTick();
    ST7796_SetAddressWindow(x, y, (uint16_t)(x + width - 1U), (uint16_t)(y + height - 1U));
    ST7796_WriteCommand(0x2C);
    dma_active = true;
    if(dma_start_chunk() != HAL_OK) {
        dma_stop();
        dma_active = false;
        dma_owner = NULL;
        return HAL_ERROR;
    }
    return HAL_OK;                  /* 这里只表示已启动，源缓冲仍属于DMA。 */
}

HAL_StatusTypeDef ST7796_WriteRGB565Wait(void)
{
    if(!dma_active) return HAL_ERROR;
    if(dma_owner != xTaskGetCurrentTaskHandle()) return HAL_BUSY;
    /* 等待信号量会让出CPU；中途被唤醒也沿用整笔事务的100ms累计期限。 */
    while(!dma_complete) {
        uint32_t elapsed = HAL_GetTick() - dma_started_ms;
        if(elapsed >= LCD_DMA_TIMEOUT_MS) break;
        (void)xSemaphoreTake(dma_done, pdMS_TO_TICKS(LCD_DMA_TIMEOUT_MS - elapsed) + 1U);
    }
    __DMB();
    HAL_StatusTypeDef result = dma_complete ? dma_result : HAL_TIMEOUT;
    /* 完成回调不是唯一依据：还核对硬件停流及HAL状态，避免假完成释放缓冲。 */
    if(result == HAL_OK && ((lcd_dma.Instance->CR & DMA_SxCR_EN) != 0U ||
       lcd_dma.State != HAL_DMA_STATE_READY || lcd_dma.ErrorCode != HAL_DMA_ERROR_NONE))
        result = HAL_ERROR;
    if(result != HAL_OK) dma_stop();
    __DSB();                       /* DMA完成包含FIFO写空；随后GUI才允许复用源数据。 */
    dma_active = false;
    dma_owner = NULL;
    return result;
}

void ST7796_WriteCommand(uint8_t cmd)
{
    if(dma_active) return;          /* 在途像素必须先Wait收尾，不能被新命令打断。 */
    ST7796_FSMC_WRITE16(LCD_COMMAND_ADDRESS, cmd);
    /* DSB是等待前面的硬件写操作结束的屏障，防止后续延时/复位先于写入生效。 */
    __DSB();
}

void ST7796_WriteData(uint8_t data)
{
    if(dma_active) return;
    ST7796_FSMC_WRITE16(LCD_DATA_ADDRESS, data);
    __DSB();
}

void ST7796_HardwareReset(void)
{
    if(dma_active) return;          /* DMA读源期间不允许复位LCD。 */
    /* 屏幕CS/RS由FSMC的PD7/PD11控制，复位只操作背光和PB1复位脚。
     * 只能在调度器已运行的GUI任务中调用，下面的延时会让出CPU。 */
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
    vTaskDelay(pdMS_TO_TICKS(10) + 1U);
    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
    vTaskDelay(pdMS_TO_TICKS(20) + 1U);
    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
    vTaskDelay(pdMS_TO_TICKS(120) + 1U);
}

void ST7796_Init(void)
{
    if(dma_active) return;
    /* main已在启动任务前执行MX_FSMC_Init。
     * 保留用户已验收的0x28方向、INVON和初始化延时，不改变屏幕工作参数。 */
    ST7796_HardwareReset();
    ST7796_WriteCommand(0x11);       /* 退出睡眠后等待内部电源/显示电路稳定。 */
    vTaskDelay(pdMS_TO_TICKS(120) + 1U);
    ST7796_WriteCommand(0x3A);
    ST7796_WriteData(0x55);          /* 16位RGB565，每个像素占两个内存字节。 */
    ST7796_WriteCommand(0x36);
    ST7796_WriteData(0x28);          /* 横屏480×320，沿用当前屏体的色序和方向。 */
    ST7796_WriteCommand(0x21);       /* INVON：沿用原屏已验证的反显设置。 */
    ST7796_WriteCommand(0x29);
    vTaskDelay(pdMS_TO_TICKS(20) + 1U);
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);
}

void ST7796_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    if(dma_active) return;
    /* 终点包含在区域内；非法参数不写屏，避免越界后LCD地址回卷。 */
    if(x0 > x1 || y0 > y1 || x1 >= ST7796_WIDTH || y1 >= ST7796_HEIGHT) return;
    ST7796_WriteCommand(0x2A);      /* 列地址：起点高/低字节，终点高/低字节。 */
    ST7796_WriteData((uint8_t)(x0 >> 8));
    ST7796_WriteData((uint8_t)x0);
    ST7796_WriteData((uint8_t)(x1 >> 8));
    ST7796_WriteData((uint8_t)x1);
    ST7796_WriteCommand(0x2B);      /* 行地址使用同样的四个8位参数。 */
    ST7796_WriteData((uint8_t)(y0 >> 8));
    ST7796_WriteData((uint8_t)y0);
    ST7796_WriteData((uint8_t)(y1 >> 8));
    ST7796_WriteData((uint8_t)y1);
}

void ST7796_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    if(dma_active) return;
    if(!width || !height || x >= ST7796_WIDTH || y >= ST7796_HEIGHT) return;
    /* 纯色填充裁剪到屏幕边缘；32位加法避免尺寸相加截断。 */
    uint32_t xe = (uint32_t)x + width, ye = (uint32_t)y + height;
    if(xe > ST7796_WIDTH) xe = ST7796_WIDTH;
    if(ye > ST7796_HEIGHT) ye = ST7796_HEIGHT;
    ST7796_SetAddressWindow(x, y, (uint16_t)(xe - 1U), (uint16_t)(ye - 1U));
    ST7796_WriteCommand(0x2C);      /* RAMWR之后，LCD自动逐像素前进。 */
    uint32_t remaining = (xe - x) * (ye - y);
    while(remaining--) ST7796_FSMC_WRITE16(LCD_DATA_ADDRESS, color);
    __DSB();                       /* 全部外部写结束后，调用方才能开始下一段操作。 */
}

void ST7796_FillScreen(uint16_t color)
{
    ST7796_FillRect(0, 0, ST7796_WIDTH, ST7796_HEIGHT, color);
}

HAL_StatusTypeDef ST7796_WriteRGB565(uint16_t x, uint16_t y,
                                    uint16_t width, uint16_t height,
                                    const uint8_t *pixels, uint32_t stride_bytes)
{
    if(dma_active) return HAL_BUSY;
    uint32_t xe = (uint32_t)x + width, ye = (uint32_t)y + height;
    if(!pixels || !width || !height || xe > ST7796_WIDTH || ye > ST7796_HEIGHT ||
       stride_bytes < (uint32_t)width * 2U) return HAL_ERROR;
    /* 不知道源对象实际容量，但可以先拒绝会使32位行偏移溢出的参数。
     * 真实容量仍由调用者保证，不能把返回HAL_OK当成LCD读回确认。 */
    if((uint64_t)(height - 1U) * stride_bytes + (uint32_t)width * 2U > UINT32_MAX)
        return HAL_ERROR;

    ST7796_SetAddressWindow(x, y, (uint16_t)(xe - 1U), (uint16_t)(ye - 1U));
    ST7796_WriteCommand(0x2C);
    /* 每行使用真实stride，跳过LVGL窄矩形的行尾填充，不修改原绘图缓冲。
     * memcpy只取一个16位值，避免把任意uint8_t指针强转造成对齐/别名问题；
     * Arm GCC -O2会内联这两个字节的读取，不分配像素搬运缓冲。
     * F407是小端CPU，内存00 F8读出的值就是0xF800，直接半字写为红色；
     * 按16位颜色值直接写入即可，不交换像素的两个字节。 */
    for(uint16_t row = 0; row < height; ++row) {
        const uint8_t *src = pixels + (uint32_t)row * stride_bytes;
        for(uint16_t col = 0; col < width; ++col) {
            uint16_t color;
            memcpy(&color, src, sizeof(color));
            ST7796_FSMC_WRITE16(LCD_DATA_ADDRESS, color);
            src += 2U;
        }
    }
    /* 同步写入：返回后已不再读取pixels，LVGL可以安全复用这块缓冲。
     * 不关中断，其他任务可抢占；LCD接口仍只由唯一GUI任务访问。 */
    __DSB();
    return HAL_OK;
}
