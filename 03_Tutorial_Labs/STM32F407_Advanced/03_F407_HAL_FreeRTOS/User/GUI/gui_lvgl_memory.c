#include "gui_lvgl_memory.h"
#include "lvgl.h"
#include "main.h"
#include <string.h>

/* NOLOAD表示不把52KiB零数据存入Flash。这里自行初始化，不改变CubeMX启动文件。
 * 池内数据只被CPU软件绘图读取；绝不能把此数组直接交给SPI或其他DMA。 */
static unsigned char pool[LV_MEM_SIZE] __attribute__((section(".lvgl_ccm"), aligned(8)));
void *GUI_LVGL_PoolAlloc(size_t bytes)
{
    if(bytes != sizeof(pool)) return NULL;
    __HAL_RCC_CCMDATARAMEN_CLK_ENABLE();
    memset(pool, 0, sizeof(pool));
    return pool;
}
