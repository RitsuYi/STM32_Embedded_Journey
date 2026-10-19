#ifndef SPI_ASYNC_MODEL_MAIN_H
#define SPI_ASYNC_MODEL_MAIN_H
#include <stdint.h>
#include <stddef.h>
/* 只模拟被驱动使用的HAL寄存器/状态。测试编译真实SPI1_Bus.c，不能代替硬件时序。 */
typedef enum {HAL_OK,HAL_ERROR,HAL_BUSY,HAL_TIMEOUT} HAL_StatusTypeDef;
typedef struct {uint32_t CR;} DMA_Stream_TypeDef;
typedef struct {DMA_Stream_TypeDef *Instance;struct {unsigned Mode;} Init;unsigned state;} DMA_HandleTypeDef;
typedef struct {struct {unsigned DataSize;} Init;DMA_HandleTypeDef *hdmatx;unsigned state,busy;} SPI_HandleTypeDef;
#define SPI_DATASIZE_8BIT 8
#define DMA_NORMAL 0
#define HAL_SPI_STATE_READY 0
#define HAL_DMA_STATE_READY 0
#define DMA_SxCR_EN 1U
#define RESET 0
#define SPI_FLAG_BSY 1
#define __DMB() ((void)0)
#define __HAL_SPI_GET_FLAG(h,f) ((void)(f),(h)->busy)
unsigned __get_IPSR(void);
unsigned HAL_SPI_GetState(SPI_HandleTypeDef *h);
unsigned HAL_DMA_GetState(DMA_HandleTypeDef *h);
HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef *h,const uint8_t *data,uint16_t size);
HAL_StatusTypeDef HAL_SPI_Abort(SPI_HandleTypeDef *h);
HAL_StatusTypeDef HAL_DMA_Abort(DMA_HandleTypeDef *h);
uint32_t HAL_GetTick(void);
void Error_Handler(void);
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *h);
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *h);
#endif
