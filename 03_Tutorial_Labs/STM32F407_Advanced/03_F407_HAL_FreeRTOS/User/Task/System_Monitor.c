#include "System_Monitor.h"
#include "Runtime_Clock.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include <string.h>

/* 大数组放普通SRAM的静态区，不放GUI任务栈，也不从RTOS堆临时申请。 */
static TaskStatus_t raw[SYSTEM_MONITOR_MAX_TASKS];
static SystemMonitorSnapshot snapshot;
typedef struct {uintptr_t id;uint32_t number;uint64_t ticks;uint16_t cpu;bool valid;} Baseline;
static Baseline baseline[SYSTEM_MONITOR_MAX_TASKS];
static uint64_t baseline_total;
static unsigned baseline_count;
static bool sampled,baseline_valid;

/* CPU忙率=1-空闲任务占比。后台只保存这两个累计计数，避免离开页面后仍扫描栈。
 * 单位为微秒，permille用千分数存储，例如368表示36.8%，不需要浮点运算。 */
static struct {
    uint64_t total,idle;
    uint32_t sample_ms,window_ms;
    uint16_t permille;
    bool sampled,valid,running;
} cpu;

void SystemMonitor_Poll(void)
{
    uint32_t now=HAL_GetTick();
    if(cpu.sampled && now-cpu.sample_ms<1000U)return;
    /* F407是32位内核；短暂保留并恢复中断屏蔽，防止64位计数读到一半任务切换。
     * 此接口只由GUI任务调用，此时IDLE没有运行，读取的是它已完整入账的时间。 */
    uint32_t mask=__get_PRIMASK();__disable_irq();
    bool running=xTaskGetSchedulerState()==taskSCHEDULER_RUNNING;
    uint64_t total=RuntimeClock_Read();
    uint64_t idle=running?ulTaskGetIdleRunTimeCounter():0U;
    __set_PRIMASK(mask);
    uint64_t duration=total,idle_delta=idle;
    if(cpu.sampled && total>cpu.total && idle>=cpu.idle) {
        duration=total-cpu.total;idle_delta=idle-cpu.idle;
    }
    cpu.valid=running && duration>0U;
    if(cpu.valid) {
        if(idle_delta>duration)idle_delta=duration;
        cpu.permille=(uint16_t)(1000U-idle_delta*1000ULL/duration);
        cpu.window_ms=(uint32_t)(duration/1000U);
    }
    cpu.total=total;cpu.idle=idle;cpu.sample_ms=now;cpu.running=running;cpu.sampled=true;
}

/* 这些地址由链接器填写，不是可解引用的普通变量；只取地址用于容量计算。 */
extern unsigned char _eram_reserved,_eccmram,_eflash_image;

static unsigned order(const char *name)
{
    const char *known[]={"GUI","Camera","OLED","Buzzer","Tmr Svc","IDLE"};
    for(unsigned i=0;i<6U;++i)if(!strcmp(name,known[i]))return i;
    return 6U;
}
const SystemMonitorSnapshot *SystemMonitor_Read(void *context)
{
    (void)context;
    uint32_t now=HAL_GetTick();
    SystemMonitor_Poll();
    snapshot.cpu_valid=cpu.valid;snapshot.cpu_permille=cpu.permille;
    snapshot.cpu_window_ms=cpu.window_ms;snapshot.scheduler_running=cpu.running;
    if(sampled && now-snapshot.sample_ms<1000U)return &snapshot;
    /* 各任务只在可见页采集；重开时先显示累计占比，再过渡到每秒差值。
     * 概览CPU由上面的轻量后台采样提供，不会因离开页面而重新等待。 */
    if(!sampled || now-snapshot.sample_ms>2000U) {
        baseline_valid=false;
    }
    sampled=true;snapshot.sample_ms=now;snapshot.uptime_ms=now;
    snapshot.heap_bytes=configTOTAL_HEAP_SIZE;
    HeapStats_t heap;vPortGetHeapStats(&heap);
    snapshot.heap_free=(uint32_t)heap.xAvailableHeapSpaceInBytes;
    snapshot.min_heap_free=(uint32_t)heap.xMinimumEverFreeBytesRemaining;
    snapshot.largest_heap_block=(uint32_t)heap.xSizeOfLargestFreeBlockInBytes;
    snapshot.ram_reserved=(uint32_t)((uintptr_t)&_eram_reserved-0x20000000U);
    snapshot.ccm_reserved=(uint32_t)((uintptr_t)&_eccmram-0x10000000U);
    snapshot.flash_used=(uint32_t)((uintptr_t)&_eflash_image-0x08000000U);

    uint64_t total=0U;
    snapshot.total_tasks=(uint8_t)uxTaskGetNumberOfTasks();
    UBaseType_t count=uxTaskGetSystemState(raw,SYSTEM_MONITOR_MAX_TASKS,&total);
    snapshot.available=true;snapshot.complete=count!=0U;
    snapshot.count=(uint8_t)count;
    if(!count) {baseline_valid=false;return &snapshot;}
    bool window=baseline_valid && total>baseline_total;
    uint64_t duration=window?total-baseline_total:total;
    TaskHandle_t self=xTaskGetCurrentTaskHandle();
    for(unsigned i=0;i<count;++i) {
        TaskStatus_t *r=&raw[i];SystemTaskInfo *t=&snapshot.tasks[i];
        *t=(SystemTaskInfo){.id=(uintptr_t)r->xHandle,.number=(uint32_t)r->xTaskNumber,
            .priority=(uint8_t)r->uxCurrentPriority,.state=(uint8_t)r->eCurrentState};
        size_t n=strlen(r->pcTaskName);if(n>=sizeof(t->name))n=sizeof(t->name)-1U;
        memcpy(t->name,r->pcTaskName,n);t->name[n]=0;
        uintptr_t low=(uintptr_t)r->pxStackBase,high=(uintptr_t)(r->pxEndOfStack+1);
        /* 其他任务取最近切换时保存的栈指针，包含已保存的上下文。
         * 当前GUI任务的TCB值已过时，须读正在使用的PSP；这是采样函数内的快照，
         * 不能保证捕获两次采样之间的瞬时峰值。历史余量仍另用栈填充值扫描结果。 */
        uintptr_t sp=r->xHandle==self?(uintptr_t)__get_PSP():(uintptr_t)r->pxTopOfStack;
        if(high>=low && high-low<=UINT32_MAX) {
            t->stack_bytes=(uint32_t)(high-low);
            t->current_valid=sp>=low && sp<=high;
            if(t->current_valid)t->used_bytes=(uint32_t)(high-sp);
        }
        t->min_free_bytes=(uint32_t)r->usStackHighWaterMark*sizeof(StackType_t);
        /* 没有前一个采样点时只能计算启动以来的占比，不能虚构一个1秒窗口。 */
        if(!baseline_valid && duration>0U) {
            uint64_t value=r->ulRunTimeCounter*1000ULL/duration;
            t->cpu_permille=(uint16_t)(value>1000U?1000U:value);t->cpu_valid=true;
        }
        if(baseline_valid)for(unsigned j=0;j<baseline_count;++j) {
            /* 句柄可被删除后复用，必须同时比较任务编号，避免新任务继承旧任务CPU。 */
            if(baseline[j].id==t->id && baseline[j].number==t->number &&
               r->ulRunTimeCounter>=baseline[j].ticks) {
                if(window) {
                    uint64_t value=(r->ulRunTimeCounter-baseline[j].ticks)*1000ULL/duration;
                    t->cpu_permille=(uint16_t)(value>1000U?1000U:value);t->cpu_valid=true;
                } else {t->cpu_permille=baseline[j].cpu;t->cpu_valid=baseline[j].valid;}
                break;
            }
        }
    }
    if(!baseline_valid||window) {
        baseline_count=count;baseline_total=total;baseline_valid=true;
        for(unsigned i=0;i<count;++i)baseline[i]=(Baseline){(uintptr_t)raw[i].xHandle,
            (uint32_t)raw[i].xTaskNumber,raw[i].ulRunTimeCounter,
            snapshot.tasks[i].cpu_permille,snapshot.tasks[i].cpu_valid};
    }
    /* 排序仅用于显示，CPU匹配已经按身份完成，不依赖FreeRTOS内部就绪列表顺序。 */
    for(unsigned i=1;i<count;++i) {
        SystemTaskInfo item=snapshot.tasks[i];unsigned j=i;
        while(j && (order(item.name)<order(snapshot.tasks[j-1U].name) ||
              (order(item.name)==order(snapshot.tasks[j-1U].name)&&strcmp(item.name,snapshot.tasks[j-1U].name)<0))) {
            snapshot.tasks[j]=snapshot.tasks[j-1U];--j;
        }
        snapshot.tasks[j]=item;
    }
    return &snapshot;
}
