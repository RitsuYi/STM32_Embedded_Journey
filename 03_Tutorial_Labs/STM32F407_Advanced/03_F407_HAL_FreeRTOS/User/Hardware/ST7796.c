#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "spi.h"
#include "ST7796.h"
#include "SPI1_Bus.h"

/* 单GUI任务只允许一个LCD事务在途；另一绘图缓冲由CPU使用，不代表第二条SPI事务。 */
static bool lcd_dma_active;

HAL_StatusTypeDef ST7796_StartRGB565DMA(uint16_t x,uint16_t y,uint16_t width,uint16_t height,
                                       const uint8_t *pixels)
{
    uint32_t xe=(uint32_t)x+width,ye=(uint32_t)y+height;
    uint32_t bytes=(uint32_t)width*height*2U;
    if(!pixels || !width || !height || xe>ST7796_WIDTH || ye>ST7796_HEIGHT || bytes>65534U)
        return HAL_ERROR;
    if(lcd_dma_active) return HAL_BUSY;
    if(!SPI1_Bus_Lock()) return HAL_ERROR;
    uint16_t x1=(uint16_t)(xe-1U),y1=(uint16_t)(ye-1U);
    uint8_t commands[]={0x2A,0x2B,0x2C};
    uint8_t params[2][4]={
        {(uint8_t)(x>>8),(uint8_t)x,(uint8_t)(x1>>8),(uint8_t)x1},
        {(uint8_t)(y>>8),(uint8_t)y,(uint8_t)(y1>>8),(uint8_t)y1}};
    HAL_StatusTypeDef status;
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port,FLASH_CS_Pin,GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,LCD_CS_Pin,GPIO_PIN_RESET);
    /* 地址命令很短，仍用轮询；有限20ms超时避免新异步链路卡在命令阶段。 */
    for(unsigned i=0;i<3;++i) {
        HAL_GPIO_WritePin(LCD_DC_GPIO_Port,LCD_DC_Pin,GPIO_PIN_RESET);
        status=HAL_SPI_Transmit(&hspi1,&commands[i],1,20);
        if(status!=HAL_OK) goto polling_failed;
        HAL_GPIO_WritePin(LCD_DC_GPIO_Port,LCD_DC_Pin,GPIO_PIN_SET);
        if(i<2) {
            status=HAL_SPI_Transmit(&hspi1,params[i],4,20);
            if(status!=HAL_OK) goto polling_failed;
        }
    }
    status=SPI1_Bus_StartDMA(pixels,(uint16_t)bytes);
    if(status==HAL_OK) {lcd_dma_active=true;return HAL_OK;}
    goto failed;
polling_failed:
    SPI1_Bus_AbortPolling(); /* 包括SPI移位寄存器未发送完的命令超时。 */
failed:
    /* Start失败时底层已安全停止自己启动的DMA；没有开始的BUSY不会误停别人的发送。 */
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,LCD_CS_Pin,GPIO_PIN_SET);
    SPI1_Bus_Unlock();return status;
}

HAL_StatusTypeDef ST7796_FinishRGB565DMA(void)
{
    if(!lcd_dma_active || !SPI1_Bus_DMAOwnedByCaller()) return HAL_ERROR;
    HAL_StatusTypeDef status=SPI1_Bus_WaitDMA(pdMS_TO_TICKS(100));
    /* Wait已经确认DMA EN和SPI BSY清零，才可取消LCD片选并归还外层总线锁。 */
    lcd_dma_active=false;
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,LCD_CS_Pin,GPIO_PIN_SET);
    SPI1_Bus_Unlock();return status;
}

void ST7796_WriteCommand(uint8_t cmd)
{
    if(!SPI1_Bus_Lock()) return;
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    SPI1_Bus_Unlock();
}

void ST7796_WriteData(uint8_t data)
{
    if(!SPI1_Bus_Lock()) return;
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
    HAL_SPI_Transmit(&hspi1, &data, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    SPI1_Bus_Unlock();
}

void ST7796_HardwareReset(void)
{
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);

  HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_RESET);

  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
  vTaskDelay(pdMS_TO_TICKS(10));
  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
  vTaskDelay(pdMS_TO_TICKS(20));
  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
  vTaskDelay(pdMS_TO_TICKS(120));
}


void ST7796_Init(void)
{
    if(!SPI1_Bus_Init()) return;
    // 硬件复位
    ST7796_HardwareReset();

    // 退出睡眠模式
    ST7796_WriteCommand(0x11);
    vTaskDelay(pdMS_TO_TICKS(120) + 1U);

    // RGB565颜色格式
    ST7796_WriteCommand(0x3A);
    ST7796_WriteData(0x55);

    // 横屏显示方向，后续通过色块验证
    ST7796_WriteCommand(0x36);
    ST7796_WriteData(0x28);

    // 当前ST7796 LCD模组经五色测试确认需要INVON
    ST7796_WriteCommand(0x21);

    // 开启显示
    ST7796_WriteCommand(0x29);
    vTaskDelay(pdMS_TO_TICKS(20) + 1U);

    // 打开背光
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);
}

//设置显示区域
void ST7796_SetAddressWindow(uint16_t x0, uint16_t y0,
                             uint16_t x1, uint16_t y1)
{
    if(!SPI1_Bus_Lock()) return;
    // 设置列地址
    ST7796_WriteCommand(0x2A);
    ST7796_WriteData(x0 >> 8);
    ST7796_WriteData(x0 & 0xFF);
    ST7796_WriteData(x1 >> 8);
    ST7796_WriteData(x1 & 0xFF);

    // 设置行地址
    ST7796_WriteCommand(0x2B);
    ST7796_WriteData(y0 >> 8);
    ST7796_WriteData(y0 & 0xFF);
    ST7796_WriteData(y1 >> 8);
    ST7796_WriteData(y1 & 0xFF);
    SPI1_Bus_Unlock();
}

//整屏填充颜色
void ST7796_FillScreen(uint16_t color)
{
    ST7796_FillRect(
        0, 0,
        ST7796_WIDTH,
        ST7796_HEIGHT,
        color
    );
}

/*
 * @brief 填充指定矩形区域
 * @param x      左上角X坐标
 * @param y      左上角Y坐标
 * @param width  矩形宽度
 * @param height 矩形高度
 * @param color  RGB565颜色
 */
void ST7796_FillRect(uint16_t x,
                     uint16_t y,
                     uint16_t width,
                     uint16_t height,
                     uint16_t color)
{
    static uint8_t buffer[512];

    // 1. 检查绘制区域是否合法
    if (width == 0 || height == 0 ||
        x >= ST7796_WIDTH ||
        y >= ST7796_HEIGHT)
    {
        return;
    }

    // 2. 防止矩形超出屏幕范围
    uint32_t xEnd = (uint32_t)x + width;
    uint32_t yEnd = (uint32_t)y + height;

    if (xEnd > ST7796_WIDTH)
        xEnd = ST7796_WIDTH;

    if (yEnd > ST7796_HEIGHT)
        yEnd = ST7796_HEIGHT;

    // 3. 计算实际需要发送的像素数量
    uint32_t remaining =
        (xEnd - x) * (yEnd - y);

    // 4. 准备256个RGB565像素
    for (uint16_t i = 0; i < 256; i++)
    {
        buffer[i * 2]     = color >> 8;
        buffer[i * 2 + 1] = color & 0xFF;
    }

    if(!SPI1_Bus_Lock()) return;

    // 5. 设置矩形显示区域
    ST7796_SetAddressWindow(
        x, y,
        (uint16_t)(xEnd - 1),
        (uint16_t)(yEnd - 1)
    );

    // 6. 确保板载Flash未被选中
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port,
                      FLASH_CS_Pin, GPIO_PIN_SET);

    // 7. 发送RAMWR命令
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,
                      LCD_CS_Pin, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_DC_GPIO_Port,
                      LCD_DC_Pin, GPIO_PIN_RESET);

    uint8_t cmd = 0x2C;

    HAL_SPI_Transmit(&hspi1, &cmd,
                     1, HAL_MAX_DELAY);

    // 8. 切换为数据模式
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port,
                      LCD_DC_Pin, GPIO_PIN_SET);

    // 9. 分块发送RGB565像素
    while (remaining > 0)
    {
        uint16_t count =
            (remaining > 256) ?
            256 : (uint16_t)remaining;

        HAL_SPI_Transmit(&hspi1,
                         buffer,
                         count * 2,
                         HAL_MAX_DELAY);

        remaining -= count;
    }

    // 10. 结束LCD通信
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,
                      LCD_CS_Pin, GPIO_PIN_SET);
    SPI1_Bus_Unlock();
}

/*
 * 将一块已有的RGB565图像写入LCD，供GUI任务的LVGL刷新回调使用。
 * RGB565表示一个像素用16位保存红、绿、蓝分量，因此每像素占2字节。
 *
 * x、y：目标矩形左上角在LCD上的坐标；width、height：矩形的像素宽高。
 * pixels：源图像首字节地址，按行存放，每个像素为低字节在前的小端格式。
 * stride_bytes：相邻两行起点之间的字节数，可能包含不显示的行尾填充字节。
 * 调用者须保证源数据至少有(height - 1) * stride_bytes + width * 2字节，
 * 且本函数返回前源数据始终有效、不被其他任务改写。
 *
 * 命令和地址参数较短，使用轮询发送；大量像素按块使用DMA发送。
 * DMA即由硬件从内存搬运数据，GUI任务通过信号量等待每块结果，CPU可运行其他任务。
 * 本函数仍按顺序执行，全部块发送完成或发生可返回的错误后才返回。
 * 只能从运行中的GUI任务调用；总线初始化须已完成，不能从中断调用。
 *
 * 返回HAL_OK表示全部像素发送成功；HAL_ERROR也可能表示参数或初始化不满足要求，
 * HAL_BUSY表示底层忙，HAL_TIMEOUT表示某次传输等待超时。
 * 若DMA异常后无法确认停止，底层封装会进入错误处理，不返回以免缓冲被提前复用。
 * 调用方应在本函数返回后再调用lv_display_flush_ready，允许LVGL复用绘图缓冲。
 */
HAL_StatusTypeDef ST7796_WriteRGB565(uint16_t x, uint16_t y,
                                    uint16_t width, uint16_t height,
                                    const uint8_t *pixels, uint32_t stride_bytes)
{
    /* 一行最多480个像素，RGB565每像素占2字节，因此预留960字节。
    * static让缓冲在DMA等待期间一直有效，不占用GUI任务栈；
    * 此缓冲须继续放在DMA可以访问的普通SRAM中。 */
    static uint8_t tx[ST7796_WIDTH * 2U];

    /* sizeof(tx)得到缓冲的字节数，除以2换算为可容纳的像素数。
    * 后续发送上限跟随缓冲容量计算，避免扩大缓冲后仍只发256像素。 */
    const uint16_t tx_capacity = (uint16_t)(sizeof(tx) / 2U);

    /* xe、ye是矩形右边/下边再加1的位置，用32位计算防止16位加法结果截断。
     * 超出屏幕、空指针、零尺寸或行步幅不足时直接拒绝；本接口不会自动裁剪。 */
    uint32_t xe = (uint32_t)x + width, ye = (uint32_t)y + height;
    if(!pixels || !width || !height || xe > ST7796_WIDTH || ye > ST7796_HEIGHT ||
       stride_bytes < (uint32_t)width * 2U) return HAL_ERROR;
    /* 从设置显示窗口到所有像素发送结束都持有同一个总线锁。
     * 即使GUI任务等待DMA时休眠，其他遵守同一锁的SPI1使用者也不能插入事务。
     * DMA封装内部再次取锁，所以这里使用工程已有的递归互斥锁。 */
    if(!SPI1_Bus_Lock()) return HAL_ERROR;

    /* LCD窗口终点为包含端点的坐标，例如x=0、width=10时终点为9。 */
    uint16_t x1 = (uint16_t)(xe - 1U), y1 = (uint16_t)(ye - 1U);

    /* 0x2A设置列范围，0x2B设置行范围，0x2C开始连续写入像素。
     * 每组地址参数均为“起点高字节、起点低字节、终点高字节、终点低字节”。 */
    uint8_t commands[] = {0x2A, 0x2B, 0x2C};
    uint8_t params[2][4] = {
        {(uint8_t)(x >> 8), (uint8_t)x, (uint8_t)(x1 >> 8), (uint8_t)x1},
        {(uint8_t)(y >> 8), (uint8_t)y, (uint8_t)(y1 >> 8), (uint8_t)y1}
    };
    HAL_StatusTypeDef status = HAL_OK;

    /* CS是低电平有效的片选信号：先取消选择Flash，再选中LCD，避免两个设备同时响应。
     * LCD_CS一直保持低电平，直到下面统一收尾；整个矩形属于一次LCD通信事务。 */
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);

    for(unsigned i = 0; i < 3; ++i) {
        /* DC低电平表示接下来是命令；轮询发送等待完成后，才能把DC改为数据模式。
         * 这些命令只有1字节，直接发送可避免每字节启动DMA及处理中断的额外开销。 */
        HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);
        status = HAL_SPI_Transmit(&hspi1, &commands[i], 1, HAL_MAX_DELAY);
        if(status != HAL_OK) goto done;
        HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
        if(i < 2) {
            /* 列/行地址各有4字节参数；0x2C没有这组参数，后续直接发送像素。
             * HAL_MAX_DELAY不限制此次轮询等待时间；发生HAL错误则转到统一收尾。 */
            status = HAL_SPI_Transmit(&hspi1, params[i], 4, HAL_MAX_DELAY);
            if(status != HAL_OK) goto done;
        }
    }

    /* 逐行取数据，用源图像实际步幅定位下一行，跳过不应发送的行尾填充字节。
     * 0x2C已使LCD进入连续写入状态，下面的各行/各块无需重复发送窗口命令。 */
    for(uint16_t row = 0; row < height; ++row) {
        const uint8_t *src = pixels + (uint32_t)row * stride_bytes;
        uint16_t remaining = width;
        while(remaining) {
            /* remaining是当前行还没发送的像素数。
            * 取它与缓冲容量中的较小值，保证本次转换不会写出tx边界。 */
            uint16_t count = remaining > tx_capacity ? tx_capacity : remaining;

            /* 将源数据的低字节在前转换为LCD需要的高字节在前。
             * 例如红色0xF800在源内存中是00 F8，发送顺序应为F8 00。
             * 这里只交换同一像素的两个字节，不改变RGB565颜色分量。 */
            for(uint16_t i = 0; i < count; ++i) {
                tx[2U*i] = src[2U*i+1U];
                tx[2U*i+1U] = src[2U*i];
            }
            /* 启动本块DMA并等待结果，HAL_OK返回时本块已经完整发送。
             * 等待期间当前GUI任务休眠，外层总线锁、LCD_CS及DC状态仍保持。
             * 底层负责超时/出错时停止本次传输；不能确认停止则不会返回。
             * 100ms只限制每块的结果等待，不包含取锁和异常收尾时间。
             * 必须收到本块结果后才能重写tx，不能仅启动DMA就继续下一轮。 */
            status = SPI1_Bus_TransmitDMA(
                tx,                        // 已完成字节交换的像素数据
                (uint16_t)(count * 2U),     // 每个RGB565像素占2字节，传入字节数
                pdMS_TO_TICKS(100));        // 将100毫秒换算为FreeRTOS系统节拍
            if(status != HAL_OK) goto done;

            /* 当前块成功后，源指针前进对应字节数，当前行剩余像素数同步减少。 */
            src += count * 2U;
            remaining -= count;
        }
    }
done:
    /* 成功和可返回的错误统一从此处收尾，避免忘记取消片选或释放锁。
     * 对本次已启动的DMA，封装在完成/安全中止后才返回，因此此时可拉高CS。
     * 先结束LCD事务再还锁，最后把结果交给上层；flush_ready由上层在返回后调用。 */
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    SPI1_Bus_Unlock();
    return status;
}
