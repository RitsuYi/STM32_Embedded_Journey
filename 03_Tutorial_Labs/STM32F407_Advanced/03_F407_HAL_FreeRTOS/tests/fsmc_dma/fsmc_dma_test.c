#include "test_platform.h"
#include "ST7796.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <setjmp.h>

#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); exit(1); } } while(0)
uint8_t dma_test_sram[128U*1024U] __attribute__((aligned(4)));
DMA_Stream_TypeDef dma_test_stream;
GPIO_TypeDef test_port;
static DMA_HandleTypeDef *dma;
static StaticSemaphore_t *semaphore;
static uint32_t now_ms,starts,fail_start,aborts,writes,received,noise_wakes;
static uint32_t chunks[400],sources[400];
static uint16_t output[65536];
static TaskHandle_t current_task=(void *)1;
static bool enabled,auto_complete=true,early_complete,abort_stuck,trap_fatal;
static enum { NORMAL,TE_ERROR,FIFO_ERROR,FALSE_COMPLETE } irq_mode;
static jmp_buf fatal_return;

/* HAL/RTOS模型：只有模拟中断才读取源；任务等待时推进中断或注入超时。 */
uint32_t HAL_GetTick(void) { return now_ms++; }
void vTaskDelay(uint32_t ticks) { now_ms+=ticks; }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return current_task; }
void Error_Handler(void)
{
    CHECK(trap_fatal); longjmp(fatal_return,1);
}
void test_lcd_write(uint32_t address,uint16_t value)
{
    (void)value;
    CHECK(address==0x60000000U || address==0x60020000U);
    CHECK(!(dma_test_stream.CR & DMA_SxCR_EN)); ++writes;
}
void HAL_GPIO_WritePin(GPIO_TypeDef *port,uint16_t pin,GPIO_PinState state)
{ (void)port; (void)pin; (void)state; }
void HAL_NVIC_SetPriority(int irq,uint32_t priority,uint32_t subpriority)
{ CHECK(irq==0 && priority>=5 && subpriority==0); }
void HAL_NVIC_ClearPendingIRQ(int irq) { CHECK(irq==0); }
void HAL_NVIC_EnableIRQ(int irq) { CHECK(irq==0); enabled=true; }
void HAL_NVIC_DisableIRQ(int irq) { CHECK(irq==0); enabled=false; }
void test_dma_interrupt(DMA_HandleTypeDef *handle,uint32_t interrupt,int on)
{
    volatile uint32_t *reg=interrupt==DMA_IT_FE ? &handle->Instance->FCR : &handle->Instance->CR;
    if(on) *reg|=interrupt; else *reg&=~interrupt;
}
void test_dma_disable(DMA_HandleTypeDef *handle)
{ if(!abort_stuck) handle->Instance->CR&=~DMA_SxCR_EN; }
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *handle)
{
    CHECK(!(handle->Instance->CR & DMA_SxCR_EN));
    CHECK(handle->Init.Direction==DMA_MEMORY_TO_MEMORY && handle->Init.PeriphInc==DMA_PINC_ENABLE);
    CHECK(handle->Init.MemInc==DMA_MINC_DISABLE && handle->Init.Mode==DMA_NORMAL);
    CHECK(handle->Init.PeriphDataAlignment==2 && handle->Init.MemDataAlignment==2);
    CHECK(handle->Init.FIFOMode==DMA_FIFOMODE_ENABLE && handle->Init.FIFOThreshold==DMA_FIFO_THRESHOLD_FULL);
    CHECK(handle->Init.MemBurst==DMA_MBURST_SINGLE && handle->Init.PeriphBurst==DMA_PBURST_SINGLE);
    dma=handle; handle->State=HAL_DMA_STATE_READY; handle->ErrorCode=HAL_DMA_ERROR_NONE;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_DMA_Start_IT(DMA_HandleTypeDef *handle,uint32_t src,uint32_t dst,uint32_t count)
{
    ++starts; CHECK(starts<400 && count>0 && count<=65535 && dst==0x60020000U);
    CHECK(handle->State==HAL_DMA_STATE_READY && !(handle->Instance->CR & DMA_SxCR_EN));
    CHECK(handle->Instance->FCR & DMA_IT_FE);
    chunks[starts]=count; sources[starts]=src;
    if(starts==fail_start) return HAL_ERROR;
    handle->Instance->PAR=src; handle->Instance->M0AR=dst; handle->Instance->NDTR=count;
    handle->Instance->CR|=DMA_SxCR_EN|DMA_IT_TC|DMA_IT_TE|DMA_IT_DME;
    handle->ErrorCode=HAL_DMA_ERROR_NONE; handle->State=HAL_DMA_STATE_BUSY;
    if(early_complete) ST7796_DMAIRQHandler();
    return HAL_OK;
}
HAL_StatusTypeDef HAL_DMA_Abort(DMA_HandleTypeDef *handle)
{
    CHECK(!enabled); ++aborts;
    if(abort_stuck) return HAL_TIMEOUT;
    handle->Instance->CR&=~DMA_SxCR_EN; handle->State=HAL_DMA_STATE_READY;
    return HAL_OK;
}
void HAL_DMA_IRQHandler(DMA_HandleTypeDef *handle)
{
    CHECK(enabled);
    if(irq_mode==TE_ERROR || irq_mode==FIFO_ERROR) {
        handle->ErrorCode=irq_mode==TE_ERROR ? HAL_DMA_ERROR_TE : HAL_DMA_ERROR_FE;
        /* FIFO错误故意保留BUSY/EN，检查任务一定会中止，不提前归还缓冲。 */
        if(irq_mode==TE_ERROR) {
            handle->Instance->CR&=~DMA_SxCR_EN; handle->State=HAL_DMA_STATE_READY;
        }
        handle->XferErrorCallback(handle); return;
    }
    if(irq_mode==FALSE_COMPLETE) { handle->XferCpltCallback(handle); return; }
    uint32_t offset=handle->Instance->PAR-(uint32_t)(uintptr_t)dma_test_sram;
    for(uint32_t i=0;i<handle->Instance->NDTR;++i) {
        CHECK(received<65536 && offset+i*2U+2U<=sizeof(dma_test_sram));
        memcpy(&output[received++],dma_test_sram+offset+i*2U,2);
    }
    handle->Instance->NDTR=0;
    handle->Instance->CR&=~(DMA_SxCR_EN|DMA_IT_TC);
    handle->State=HAL_DMA_STATE_READY; handle->XferCpltCallback(handle);
}
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *storage)
{ semaphore=storage; storage->token=0; return storage; }
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t handle,BaseType_t *woken)
{ handle->token=1; *woken=0; return pdTRUE; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t handle,uint32_t ticks)
{
    if(ticks && noise_wakes) { --noise_wakes; ++now_ms; return pdFALSE; }
    if(ticks && !handle->token) {
        if(auto_complete) {
            while(!handle->token && (dma_test_stream.CR & DMA_SxCR_EN)) ST7796_DMAIRQHandler();
        } else now_ms+=ticks;
    }
    if(!handle->token) return pdFALSE;
    handle->token=0; return pdTRUE;
}
static HAL_StatusTypeDef start(uint16_t w,uint16_t h,uint32_t stride)
{
    received=0;
    return ST7796_WriteRGB565Start(0,0,w,h,dma_test_sram,stride);
}
static void successful_transfer(void)
{
    CHECK(start(3,2,6)==HAL_OK); CHECK(ST7796_WriteRGB565Wait()==HAL_OK);
    CHECK(received==6 && !(dma_test_stream.CR & DMA_SxCR_EN));
}
int main(void)
{
    CHECK(ST7796_DMAInit()==HAL_OK);
    CHECK(ST7796_WriteRGB565Wait()==HAL_ERROR);
    const uint16_t pixels[]={0xF800,0x07E0,0x001F,0xFFFF,0x1234,0xABCD};
    memcpy(dma_test_sram,pixels,sizeof(pixels));

    /* Start必须立即返回，直到Wait才消费源；在途重复调用不发命令也不误停。 */
    CHECK(start(3,2,6)==HAL_OK && received==0);
    uint32_t before_writes=writes,before_aborts=aborts;
    CHECK(start(3,2,6)==HAL_BUSY);
    CHECK(ST7796_WriteRGB565(0,0,3,2,dma_test_sram,6)==HAL_BUSY);
    ST7796_FillScreen(0); ST7796_WriteCommand(0x2C);
    CHECK(writes==before_writes && aborts==before_aborts);
    current_task=(void *)2;
    CHECK(ST7796_WriteRGB565Wait()==HAL_BUSY);
    current_task=(void *)1;
    CHECK(ST7796_WriteRGB565Wait()==HAL_OK && received==6);
    CHECK(memcmp(output,pixels,sizeof(pixels))==0); /* 原生565，不能交换字节。 */

    /* 3像素一行、8字节步长：跳过0xDEAD行尾，窗口只设置一次。 */
    const uint16_t padded[]={1,2,3,0xDEAD,4,5,6};
    memcpy(dma_test_sram,padded,sizeof(padded));
    uint32_t base=starts;
    CHECK(start(3,2,8)==HAL_OK); CHECK(ST7796_WriteRGB565Wait()==HAL_OK);
    CHECK(starts==base+2 && chunks[base+1]==3 && chunks[base+2]==3);
    CHECK(sources[base+2]-sources[base+1]==8 && output[3]==4 && output[5]==6);

    /* 一个65536像素的合法SRAM对象必须拆成65535+1，不能截断NDTR。 */
    base=starts; CHECK(start(256,256,512)==HAL_OK);
    CHECK(ST7796_WriteRGB565Wait()==HAL_OK && received==65536);
    CHECK(chunks[base+1]==65535 && chunks[base+2]==1);
    CHECK(sources[base+2]-sources[base+1]==131070U);

    /* 地址/对齐/stride/屏幕越界必须在任何LCD命令或DMA启动前拒绝。 */
    base=starts; before_writes=writes;
    CHECK(ST7796_WriteRGB565Start(0,0,1,1,NULL,2)==HAL_ERROR);
    CHECK(ST7796_WriteRGB565Start(0,0,1,1,(void *)0x10000000U,2)==HAL_ERROR);
    CHECK(ST7796_WriteRGB565Start(0,0,1,1,dma_test_sram+1,2)==HAL_ERROR);
    CHECK(ST7796_WriteRGB565Start(0,0,3,2,dma_test_sram,7)==HAL_ERROR);
    CHECK(ST7796_WriteRGB565Start(0,0,3,2,dma_test_sram,4)==HAL_ERROR);
    CHECK(ST7796_WriteRGB565Start(479,0,2,1,dma_test_sram,4)==HAL_ERROR);
    CHECK(ST7796_WriteRGB565Start(0,0,1,2,dma_test_sram+sizeof(dma_test_sram)-2,2)==HAL_ERROR);
    CHECK(ST7796_WriteRGB565Start(0,0,3,320,dma_test_sram,UINT32_MAX-1U)==HAL_ERROR);
    CHECK(starts==base && writes==before_writes);

    early_complete=true; successful_transfer(); early_complete=false;
    semaphore->token=1; noise_wakes=3; successful_transfer(); /* 提前/遗留/无效唤醒。 */
    now_ms=UINT32_MAX-20U; successful_transfer(); /* 毫秒计数回绕。 */

    fail_start=starts+1; CHECK(start(3,2,6)==HAL_ERROR);
    fail_start=0; successful_transfer();
    fail_start=starts+2; CHECK(start(3,2,8)==HAL_OK);
    CHECK(ST7796_WriteRGB565Wait()==HAL_ERROR); fail_start=0; successful_transfer();
    irq_mode=TE_ERROR; CHECK(start(3,2,6)==HAL_OK); CHECK(ST7796_WriteRGB565Wait()==HAL_ERROR);
    irq_mode=NORMAL; successful_transfer();
    irq_mode=FIFO_ERROR; before_aborts=aborts;
    CHECK(start(3,2,6)==HAL_OK); CHECK(ST7796_WriteRGB565Wait()==HAL_ERROR && aborts>before_aborts);
    irq_mode=NORMAL; successful_transfer();
    irq_mode=FALSE_COMPLETE; CHECK(start(3,2,6)==HAL_OK);
    CHECK(ST7796_WriteRGB565Wait()==HAL_ERROR); irq_mode=NORMAL; successful_transfer();
    auto_complete=false; now_ms=UINT32_MAX-30U;
    CHECK(start(3,2,6)==HAL_OK); CHECK(ST7796_WriteRGB565Wait()==HAL_TIMEOUT);
    auto_complete=true; successful_transfer();

    /* 硬件拒绝停流时必须进入保护，Wait不能正常返回并让调用方覆盖源缓冲。 */
    auto_complete=false; abort_stuck=true; trap_fatal=true;
    if(setjmp(fatal_return)==0) {
        CHECK(start(3,2,6)==HAL_OK); (void)ST7796_WriteRGB565Wait();
        CHECK(false);
    }
    CHECK(dma_test_stream.CR & DMA_SxCR_EN);
    CHECK(start(3,2,6)==HAL_BUSY);
    puts("PASS: DMA config, native565/stride/chunks, source bounds, ownership, early/stale wake, wrap, start/IRQ/timeout/stop failure");
    return 0;
}
