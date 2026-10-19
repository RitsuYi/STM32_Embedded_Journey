#include "SPI1_Bus.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#define CHECK(c) do {if(!(c)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);exit(1);}}while(0)
static DMA_Stream_TypeDef stream;
static DMA_HandleTypeDef dma={.Instance=&stream};
SPI_HandleTypeDef hspi1={.Init={SPI_DATASIZE_8BIT},.hdmatx=&dma};
static SemaphoreHandle_t mutex;
static unsigned irq,scheduler=1,owner=1,starts,aborts,dma_aborts,mode;
static TickType_t ticks,wait_ticks;
static HAL_StatusTypeDef start_status=HAL_OK;
static bool immediate,abort_fails;
static jmp_buf stopped;
/* mode=0成功；1错误且DMA EN残留；2丢中断；3假完成但SPI仍忙。 */
static void complete(void)
{
    if(mode==2) return;
    if(mode!=1&&mode!=3) {stream.CR=0;dma.state=0;hspi1.state=0;hspi1.busy=0;}
    irq=1;
    if(mode==1) HAL_SPI_ErrorCallback(&hspi1);else HAL_SPI_TxCpltCallback(&hspi1);
    irq=0;
}
unsigned __get_IPSR(void){return irq;}
BaseType_t xTaskGetSchedulerState(void){return (int)scheduler;}
TaskHandle_t xTaskGetCurrentTaskHandle(void){return (void *)(uintptr_t)owner;}
TickType_t xTaskGetTickCount(void){return ticks;}
uint32_t HAL_GetTick(void){return ticks++;}
unsigned HAL_SPI_GetState(SPI_HandleTypeDef *h){return h->state;}
unsigned HAL_DMA_GetState(DMA_HandleTypeDef *h){return h->state;}
SemaphoreHandle_t xSemaphoreCreateRecursiveMutexStatic(StaticSemaphore_t *s){s->depth=0;mutex=s;return s;}
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *s){s->value=0;return s;}
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t s,TickType_t timeout){(void)timeout;++s->depth;return pdTRUE;}
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t s){CHECK(s->depth>0);--s->depth;return pdTRUE;}
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t s,BaseType_t *woken){CHECK(irq);s->value=1;*woken=1;return pdTRUE;}
BaseType_t xSemaphoreTake(SemaphoreHandle_t s,TickType_t timeout)
{
    if(!s->value&&timeout){wait_ticks=timeout;complete();if(!s->value)ticks+=timeout;}
    if(s->value){s->value=0;return pdTRUE;}return pdFALSE;
}
HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef *h,const uint8_t *data,uint16_t size)
{
    CHECK(data&&size&&mutex->depth>0);++starts;
    if(start_status!=HAL_OK)return start_status;
    h->busy=1;h->state=1;dma.state=1;stream.CR=DMA_SxCR_EN;
    if(immediate)complete();
    return HAL_OK;
}
HAL_StatusTypeDef HAL_SPI_Abort(SPI_HandleTypeDef *h)
{
    ++aborts;if(abort_fails)return HAL_ERROR;
    h->busy=0;h->state=0; /* 故意不清DMA EN，验证驱动独立检查通道。 */
    return HAL_OK;
}
HAL_StatusTypeDef HAL_DMA_Abort(DMA_HandleTypeDef *h){++dma_aborts;h->Instance->CR=0;h->state=0;return HAL_OK;}
void Error_Handler(void){longjmp(stopped,1);}
static void reset(void)
{
    CHECK(!mutex->depth);irq=0;scheduler=1;owner=1;starts=aborts=dma_aborts=0;
    mode=0;start_status=HAL_OK;immediate=abort_fails=false;wait_ticks=0;
    stream.CR=0;hspi1.busy=hspi1.state=dma.state=0;
}
int main(void)
{
    uint8_t pixels[8]={0};CHECK(SPI1_Bus_Init());
    CHECK(SPI1_Bus_DMAAddressValid(0x20000000U,131072));
    CHECK(!SPI1_Bus_DMAAddressValid(0x10000000U,8));
    CHECK(!SPI1_Bus_DMAAddressValid(0x2001FFFFU,2));
    CHECK(!SPI1_Bus_DMAAddressValid(UINTPTR_MAX-3,8));
    CHECK(SPI1_Bus_DMAAddressValid(0x08000000U,1048576));
    CHECK(!SPI1_Bus_DMAAddressValid(0x080FFFFFU,2));
    reset();irq=1;CHECK(SPI1_Bus_StartDMA(pixels,8)==HAL_ERROR);irq=0;
    scheduler=0;CHECK(SPI1_Bus_StartDMA(pixels,8)==HAL_ERROR);scheduler=1;
    CHECK(SPI1_Bus_StartDMA(NULL,8)==HAL_ERROR);CHECK(SPI1_Bus_StartDMA(pixels,0)==HAL_ERROR);
    hspi1.Init.DataSize=16;CHECK(SPI1_Bus_StartDMA(pixels,8)==HAL_ERROR);hspi1.Init.DataSize=8;
    dma.Init.Mode=1;CHECK(SPI1_Bus_StartDMA(pixels,8)==HAL_ERROR);dma.Init.Mode=0;
    reset();CHECK(SPI1_Bus_Lock());CHECK(SPI1_Bus_StartDMA(pixels,8)==HAL_OK);CHECK(mutex->depth==2);
    CHECK(SPI1_Bus_StartDMA(pixels,8)==HAL_BUSY&&starts==1&&aborts==0&&mutex->depth==2);
    owner=2;CHECK(SPI1_Bus_WaitDMA(100)==HAL_ERROR&&mutex->depth==2);owner=1;
    ticks+=30;CHECK(SPI1_Bus_WaitDMA(100)==HAL_OK&&wait_ticks==70&&mutex->depth==1);
    SPI1_Bus_Unlock();CHECK(!stream.CR&&!hspi1.busy);
    reset();immediate=true;CHECK(SPI1_Bus_TransmitDMA(pixels,8,100)==HAL_OK&&aborts==0);
    /* 旧/外来回调不能替下一块发送制造完成；已到达的真实完成不会漏信号。 */
    irq=1;HAL_SPI_TxCpltCallback(&hspi1);irq=0;
    reset();CHECK(SPI1_Bus_StartDMA(pixels,8)==HAL_OK);SPI_HandleTypeDef other={0};
    irq=1;HAL_SPI_ErrorCallback(&other);irq=0;CHECK(SPI1_Bus_WaitDMA(100)==HAL_OK);
    reset();start_status=HAL_BUSY;CHECK(SPI1_Bus_StartDMA(pixels,8)==HAL_BUSY&&aborts==0&&!mutex->depth);
    reset();start_status=HAL_ERROR;CHECK(SPI1_Bus_StartDMA(pixels,8)==HAL_ERROR&&aborts==1&&!mutex->depth);
    for(unsigned failure=1;failure<=3;++failure) {
        reset();mode=failure;CHECK(SPI1_Bus_StartDMA(pixels,8)==HAL_OK);
        CHECK(SPI1_Bus_WaitDMA(100)==(failure==2?HAL_TIMEOUT:HAL_ERROR));
        CHECK(aborts==1&&dma_aborts==1&&!mutex->depth&&!stream.CR&&!hspi1.busy);
        mode=0;CHECK(SPI1_Bus_TransmitDMA(pixels,8,100)==HAL_OK);
    }
    reset();mode=2;ticks=UINT32_MAX-40;CHECK(SPI1_Bus_StartDMA(pixels,8)==HAL_OK);
    ticks+=101;CHECK(SPI1_Bus_WaitDMA(100)==HAL_TIMEOUT&&wait_ticks==0&&!mutex->depth);
    reset();mode=2;abort_fails=true;CHECK(SPI1_Bus_StartDMA(pixels,8)==HAL_OK);
    if(setjmp(stopped)==0){(void)SPI1_Bus_WaitDMA(100);CHECK(false);}
    CHECK(mutex->depth==1&&stream.CR); /* 无法停止时不解锁/归还源缓冲，保持原停机保护。 */
    puts("PASS: DMA ownership, early/foreign callbacks, deadlines/wrap, BUSY, errors/abort, EN/BSY and SRAM/CCM bounds");
    return 0;
}
