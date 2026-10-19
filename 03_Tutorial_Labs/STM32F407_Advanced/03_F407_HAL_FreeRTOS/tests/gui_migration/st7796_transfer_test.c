#include "ST7796.h"
#include "SPI1_Bus.h"
#include "main.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if(!(condition)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); } } while(0)
GPIO_TypeDef test_port;
SPI_HandleTypeDef hspi1;
static GPIO_PinState pins[6];
static unsigned locked, calls, fail_at, dma_calls;
static bool lock_fails;
static uint8_t transmitted[65545];
static HAL_StatusTypeDef wait_status=HAL_OK;
static unsigned transmitted_count;
bool SPI1_Bus_Init(void) { return true; }
bool SPI1_Bus_Lock(void) { if(lock_fails) return false; ++locked; return true; }
void SPI1_Bus_Unlock(void) { CHECK(locked); --locked; }
void vTaskDelay(uint32_t ticks) { (void)ticks; }
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
    (void)port; CHECK(pin < 6); pins[pin] = state;
}
/* HAL和DMA等待替身共用字节捕获；编译的仍是真实ST7796_WriteRGB565。
 * 检查完整事务持锁/独占片选、发送顺序，以及每次失败后的收尾。 */
static HAL_StatusTypeDef capture(const uint8_t *data,uint16_t count)
{
    CHECK(locked && pins[LCD_CS_Pin] == GPIO_PIN_RESET && pins[FLASH_CS_Pin] == GPIO_PIN_SET);
    CHECK(count);
    ++calls;
    if(calls == fail_at) return HAL_ERROR;
    CHECK(transmitted_count + count <= sizeof(transmitted));
    memcpy(transmitted + transmitted_count, data, count); transmitted_count += count;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *spi,const uint8_t *data,uint16_t count,uint32_t timeout)
{(void)spi;(void)timeout;return capture(data,count);}
HAL_StatusTypeDef SPI1_Bus_TransmitDMA(const uint8_t *data,uint16_t count,TickType_t timeout)
{
    CHECK(timeout==pdMS_TO_TICKS(100));CHECK(pins[LCD_DC_Pin]==GPIO_PIN_SET);
    ++dma_calls;return capture(data,count);
}
/* 原同步接口回归仍保留；新增异步接口在后面的专用总线/显示测试中注入时序。 */
HAL_StatusTypeDef SPI1_Bus_StartDMA(const uint8_t *data,uint16_t count)
{++dma_calls;return capture(data,count);}
HAL_StatusTypeDef SPI1_Bus_WaitDMA(TickType_t timeout)
{CHECK(timeout==pdMS_TO_TICKS(100));return wait_status;}
bool SPI1_Bus_DMAOwnedByCaller(void){return true;}
void SPI1_Bus_AbortPolling(void){CHECK(locked);}
static void reset(unsigned failure)
{
    for(unsigned i = 0; i < 6; ++i) pins[i] = GPIO_PIN_SET;
    calls = transmitted_count = locked = dma_calls = 0; fail_at = failure; lock_fails = false;wait_status=HAL_OK;
}
int main(void)
{
    /* Two rows; the last two source bytes of each row are padding, not pixels. */
    const uint8_t source[] = {0x00,0xF8,0xE0,0x07,0x1F,0x00,0xAA,0xBB,
                             0xFF,0xFF,0x00,0x00,0xE0,0xFF,0xCC,0xDD};
    const uint8_t expected[] = {0x2A,0x00,0x0A,0x00,0x0C,0x2B,0x00,0x14,0x00,0x15,0x2C,
                               0xF8,0x00,0x07,0xE0,0x00,0x1F,0xFF,0xFF,0x00,0x00,0xFF,0xE0};
    reset(0);
    CHECK(ST7796_WriteRGB565(10,20,3,2,source,8) == HAL_OK);
    CHECK(transmitted_count == sizeof(expected) && !memcmp(transmitted, expected, sizeof(expected)));
    CHECK(!locked && pins[LCD_CS_Pin] == GPIO_PIN_SET);
    unsigned transfer_count = calls;
    for(unsigned i = 1; i <= transfer_count; ++i) {
        reset(i); CHECK(ST7796_WriteRGB565(10,20,3,2,source,8) == HAL_ERROR);
        CHECK(calls == i && !locked && pins[LCD_CS_Pin] == GPIO_PIN_SET);
    }
    reset(0);
    CHECK(ST7796_WriteRGB565(479,0,2,1,source,4) == HAL_ERROR);
    CHECK(ST7796_WriteRGB565(0,319,1,2,source,2) == HAL_ERROR);
    CHECK(ST7796_WriteRGB565(0,0,0,1,source,2) == HAL_ERROR);
    CHECK(ST7796_WriteRGB565(0,0,3,1,source,4) == HAL_ERROR);
    CHECK(ST7796_WriteRGB565(0,0,1,1,NULL,2) == HAL_ERROR);
    CHECK(!calls && !locked);
    lock_fails = true; CHECK(ST7796_WriteRGB565(0,0,1,1,source,2) == HAL_ERROR); CHECK(!calls);
    /* 完整480像素行带2字节padding，跨两行验证字节交换和源步幅；每行只能一次DMA。 */
    uint8_t wide[2*962];
    for(unsigned i = 0; i < sizeof(wide); ++i) wide[i] = (uint8_t)i;
    reset(0); CHECK(ST7796_WriteRGB565(0,0,480,2,wide,962) == HAL_OK);
    CHECK(transmitted_count == 11 + 480*2*2 && dma_calls==2);
    for(unsigned row = 0; row < 2; ++row) for(unsigned pixel = 0; pixel < 480; ++pixel) {
        unsigned src = row*962 + pixel*2, dst = 11 + row*960 + pixel*2;
        CHECK(transmitted[dst] == wide[src+1] && transmitted[dst+1] == wide[src]);
    }
    /* 新接口直接读取LCD字节序的连续块；CS和外层锁必须跨越Start返回直到Finish。 */
    static uint8_t block[65534];for(unsigned i=0;i<sizeof(block);++i)block[i]=(uint8_t)i;
    reset(0);CHECK(ST7796_StartRGB565DMA(0,0,480,8,block)==HAL_OK);
    CHECK(dma_calls==1&&transmitted_count==11+7680&&locked==1&&pins[LCD_CS_Pin]==GPIO_PIN_RESET);
    CHECK(!memcmp(transmitted+11,block,7680));unsigned before=calls;
    CHECK(ST7796_StartRGB565DMA(0,8,480,8,block)==HAL_BUSY&&calls==before&&locked==1);
    CHECK(ST7796_FinishRGB565DMA()==HAL_OK&&!locked&&pins[LCD_CS_Pin]==GPIO_PIN_SET);
    CHECK(ST7796_FinishRGB565DMA()==HAL_ERROR);
    for(unsigned i=1;i<=6;++i) {
        reset(i);CHECK(ST7796_StartRGB565DMA(1,2,3,4,block)==HAL_ERROR);
        CHECK(calls==i&&!locked&&pins[LCD_CS_Pin]==GPIO_PIN_SET);
    }
    reset(0);CHECK(ST7796_StartRGB565DMA(0,0,217,151,block)==HAL_OK); /* 恰好65534字节。 */
    CHECK(dma_calls==1&&transmitted_count==65545);wait_status=HAL_TIMEOUT;
    CHECK(ST7796_FinishRGB565DMA()==HAL_TIMEOUT&&!locked&&pins[LCD_CS_Pin]==GPIO_PIN_SET);
    reset(0);CHECK(ST7796_StartRGB565DMA(0,0,256,128,block)==HAL_ERROR); /* 65536不能截成uint16。 */
    CHECK(ST7796_StartRGB565DMA(479,319,2,1,block)==HAL_ERROR&&!calls);
    puts("PASS: native/padded RGB565 and contiguous DMA, exact 65534/65536 limit, window, CS/lock lifetime, errors");
    return 0;
}
