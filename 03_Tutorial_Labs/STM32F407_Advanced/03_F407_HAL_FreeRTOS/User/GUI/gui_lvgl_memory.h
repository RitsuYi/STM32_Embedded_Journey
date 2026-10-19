#ifndef GUI_LVGL_MEMORY_H
#define GUI_LVGL_MEMORY_H
#include <stddef.h>
/* LVGL初始化时申请唯一固定池；bytes必须等于配置容量，失败返回NULL触发原内存保护。 */
void *GUI_LVGL_PoolAlloc(size_t bytes);
#endif
