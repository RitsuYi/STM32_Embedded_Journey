#ifndef FSMC_DMA_TEST_PLATFORM_H
#define FSMC_DMA_TEST_PLATFORM_H
#include <stdint.h>
#include <stddef.h>

/* 测试只提供驱动需要的平台接口，数值无需映射到真实PC硬件寄存器。 */
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef enum { HAL_DMA_STATE_RESET, HAL_DMA_STATE_READY, HAL_DMA_STATE_BUSY } HAL_DMA_StateTypeDef;
typedef struct { volatile uint32_t CR,FCR,PAR,M0AR,NDTR; } DMA_Stream_TypeDef;
typedef struct {
    uint32_t Channel,Direction,PeriphInc,MemInc,PeriphDataAlignment,MemDataAlignment;
    uint32_t Mode,Priority,FIFOMode,FIFOThreshold,MemBurst,PeriphBurst;
} DMA_InitTypeDef;
typedef struct DMA_HandleTypeDef DMA_HandleTypeDef;
struct DMA_HandleTypeDef {
    DMA_Stream_TypeDef *Instance;
    DMA_InitTypeDef Init;
    HAL_DMA_StateTypeDef State;
    uint32_t ErrorCode;
    void (*XferCpltCallback)(DMA_HandleTypeDef *);
    void (*XferErrorCallback)(DMA_HandleTypeDef *);
};
typedef int BaseType_t;
typedef void *TaskHandle_t;
typedef struct { int token; } StaticSemaphore_t;
typedef StaticSemaphore_t *SemaphoreHandle_t;
typedef enum { GPIO_PIN_RESET,GPIO_PIN_SET } GPIO_PinState;
typedef unsigned GPIO_TypeDef;

extern uint8_t dma_test_sram[128U*1024U];
extern DMA_Stream_TypeDef dma_test_stream;
extern GPIO_TypeDef test_port;
#define SRAM_BASE ((uintptr_t)dma_test_sram)
#define DMA2_Stream0 (&dma_test_stream)
#define DMA2_Stream0_IRQn 0
#define DMA_CHANNEL_0 0U
#define DMA_MEMORY_TO_MEMORY 2U
#define DMA_PINC_ENABLE 1U
#define DMA_MINC_DISABLE 0U
#define DMA_PDATAALIGN_HALFWORD 2U
#define DMA_MDATAALIGN_HALFWORD 2U
#define DMA_NORMAL 0U
#define DMA_PRIORITY_LOW 0U
#define DMA_FIFOMODE_ENABLE 1U
#define DMA_FIFO_THRESHOLD_FULL 3U
#define DMA_MBURST_SINGLE 0U
#define DMA_PBURST_SINGLE 0U
#define DMA_SxCR_EN 1U
#define DMA_IT_TC 2U
#define DMA_IT_TE 4U
#define DMA_IT_DME 8U
#define DMA_IT_HT 16U
#define DMA_IT_FE 128U
#define DMA_FLAG_FEIF0_4 1U
#define DMA_FLAG_DMEIF0_4 4U
#define DMA_FLAG_TEIF0_4 8U
#define DMA_FLAG_HTIF0_4 16U
#define DMA_FLAG_TCIF0_4 32U
#define HAL_DMA_ERROR_NONE 0U
#define HAL_DMA_ERROR_TE 1U
#define HAL_DMA_ERROR_FE 2U
#define LCD_BL_GPIO_Port (&test_port)
#define LCD_RST_GPIO_Port (&test_port)
#define LCD_BL_Pin 1U
#define LCD_RST_Pin 2U
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) (ms)
#define portYIELD_FROM_ISR(woken) ((void)(woken))
#define __DMB() ((void)0)
#define __DSB() ((void)0)
#define __HAL_RCC_DMA2_CLK_ENABLE() ((void)0)
#define __HAL_DMA_ENABLE_IT(h,it) test_dma_interrupt((h),(it),1)
#define __HAL_DMA_DISABLE_IT(h,it) test_dma_interrupt((h),(it),0)
#define __HAL_DMA_DISABLE(h) test_dma_disable(h)
#define __HAL_DMA_CLEAR_FLAG(h,flags) ((void)(h),(void)(flags))
/* 注入LCD写入接收端，不在PC上解引用0x60000000硬件地址。 */
#define ST7796_FSMC_WRITE16(address,value) test_lcd_write((address),(value))

uint32_t HAL_GetTick(void);
void Error_Handler(void);
void vTaskDelay(uint32_t ticks);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *storage);
BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore,uint32_t ticks);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t semaphore,BaseType_t *woken);
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *handle);
HAL_StatusTypeDef HAL_DMA_Start_IT(DMA_HandleTypeDef *handle,uint32_t src,uint32_t dst,uint32_t count);
HAL_StatusTypeDef HAL_DMA_Abort(DMA_HandleTypeDef *handle);
void HAL_DMA_IRQHandler(DMA_HandleTypeDef *handle);
void HAL_NVIC_SetPriority(int irq,uint32_t priority,uint32_t subpriority);
void HAL_NVIC_ClearPendingIRQ(int irq);
void HAL_NVIC_EnableIRQ(int irq);
void HAL_NVIC_DisableIRQ(int irq);
void HAL_GPIO_WritePin(GPIO_TypeDef *port,uint16_t pin,GPIO_PinState state);
void test_dma_interrupt(DMA_HandleTypeDef *handle,uint32_t interrupt,int enabled);
void test_dma_disable(DMA_HandleTypeDef *handle);
void test_lcd_write(uint32_t address,uint16_t value);
#endif
