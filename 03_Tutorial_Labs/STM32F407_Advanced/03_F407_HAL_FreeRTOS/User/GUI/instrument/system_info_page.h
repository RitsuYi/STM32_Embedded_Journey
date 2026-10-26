#ifndef SYSTEM_INFO_PAGE_H
#define SYSTEM_INFO_PAGE_H
#include "lvgl/lvgl.h"
#include "../../Task/System_Monitor.h"

/* 后端以普通快照供UI读取，电脑验证可替换数据源，正式设备绑定SystemMonitor_Read。
 * 所有方法均只由唯一GUI任务调用，其他任务不得直接更新控件。 */
typedef const SystemMonitorSnapshot *(*SystemInfoRead)(void *context);
typedef struct {
    lv_obj_t *root,*content,*list,*title,*tabs[4],*rows[SYSTEM_MONITOR_MAX_TASKS];
    uintptr_t row_ids[SYSTEM_MONITOR_MAX_TASKS],selected;
    unsigned row_count,tab;
    uint32_t last_refresh_ms; /* 从本页上次刷新计时，避免受顶部时钟整秒边界影响。 */
    bool persistent;
    SystemInfoRead read;
    void *context;
    const SystemMonitorSnapshot *snapshot;
    lv_mem_monitor_t memory;
} SystemInfoPage;

void system_info_create(SystemInfoPage *page,lv_obj_t *parent,bool persistent);
void system_info_refresh(SystemInfoPage *page);
/* 父菜单清理前解除所有借用对象指针；对象本身由父菜单统一释放。 */
void system_info_detach(SystemInfoPage *page);
/* 任务详情页的返回先回任务表，普通标签页的返回交给原设置菜单。 */
bool system_info_back(SystemInfoPage *page);
#endif
