#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* 固定快照最多容纳12任务，当前只有6个；容量不足明确报错，不截断后伪称完整。
 * 此头文件仅含普通数据，UI/电脑验证也能使用，不把RTOS内部指针暴露给绘图代码。 */
#define SYSTEM_MONITOR_MAX_TASKS 12U
typedef struct {
    uintptr_t id;
    uint32_t number;
    char name[16];
    uint32_t stack_bytes,used_bytes,min_free_bytes;
    uint16_t cpu_permille;
    uint8_t priority,state;
    bool current_valid,cpu_valid;
} SystemTaskInfo;
typedef struct {
    SystemTaskInfo tasks[SYSTEM_MONITOR_MAX_TASKS];
    uint32_t uptime_ms,sample_ms,cpu_window_ms;
    uint32_t heap_bytes,heap_free,min_heap_free,largest_heap_block;
    uint32_t ram_reserved,ccm_reserved,flash_used;
    uint16_t cpu_permille;
    uint8_t count,total_tasks;
    bool available,complete,cpu_valid,scheduler_running;
} SystemMonitorSnapshot;

/* GUI任务循环调用：内部每秒只读一次空闲任务计数，不扫描栈、不申请内存。
 * 页面关闭时也保留最近CPU结果，进入页面便可显示；不需要新增后台任务。 */
void SystemMonitor_Poll(void);

/* 仅GUI任务调用：可见页最多每秒扫描一次栈和各任务计数。
 * 返回模块拥有的只读快照，调用者不得释放。首次采样用启动以来的真实累计值，
 * 后续CPU使用相邻采样差值；计时器尚无有效时长时才显示不可用。 */
const SystemMonitorSnapshot *SystemMonitor_Read(void *context);
#endif
