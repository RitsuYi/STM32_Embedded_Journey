#include "System_Monitor.h"
#include "Runtime_Clock.h"
#include "task.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);exit(1);}}while(0)
FakeRCC fake_rcc;FakeTIM fake_tim;unsigned fake_frozen,fake_mask;
unsigned char _eram_reserved,_eccmram,_eflash_image;
static uint32_t ms;static uint64_t ticks;
static StackType_t stacks[3][256];static TaskStatus_t tasks[3];
static unsigned reads,idle_reads,total_tasks=3;
static bool scheduler_running=true;
uint32_t HAL_GetTick(void){return ms;}
uint32_t HAL_RCC_GetPCLK1Freq(void){return 42000000U;}
uintptr_t __get_PSP(void){return (uintptr_t)&stacks[0][226];}
void Error_Handler(void){CHECK(0);}
void vPortGetHeapStats(HeapStats_t *h){*h=(HeapStats_t){10000U,9000U,8000U};}
UBaseType_t uxTaskGetNumberOfTasks(void){return total_tasks;}
TaskHandle_t xTaskGetCurrentTaskHandle(void){return (void *)(uintptr_t)1;}
TaskHandle_t xTaskGetIdleTaskHandle(void){return (void *)(uintptr_t)3;}
int xTaskGetSchedulerState(void){return scheduler_running?taskSCHEDULER_RUNNING:0;}
uint64_t ulTaskGetIdleRunTimeCounter(void){++idle_reads;return tasks[2].ulRunTimeCounter;}
UBaseType_t uxTaskGetSystemState(TaskStatus_t *out,UBaseType_t capacity,uint64_t *total)
{
    ++reads;if(capacity<total_tasks)return 0;
    /* 每次改变内部枚举顺序，验证按任务身份匹配，而非错误地按第几行相减。 */
    for(unsigned i=0;i<3;++i)out[i]=tasks[(i+reads)%3];
    *total=ticks;return 3;
}
static const SystemMonitorSnapshot *sample(uint32_t time,uint64_t clock)
{
    ms=time;ticks=clock;fake_tim.CNT=(uint32_t)clock;
    tasks[0].ulRunTimeCounter=clock*35U/100U;
    tasks[1].ulRunTimeCounter=clock/10U;
    tasks[2].ulRunTimeCounter=clock*55U/100U;
    return SystemMonitor_Read(NULL);
}
int main(void)
{
    fake_rcc.CFGR=RCC_CFGR_PPRE1;RuntimeClock_Init();
    CHECK(fake_tim.PSC==83U&&fake_tim.ARR==UINT32_MAX&&fake_frozen==1U);
    fake_mask=0;fake_tim.CNT=UINT32_MAX-5U;
    CHECK(RuntimeClock_Read()==UINT32_MAX-5ULL&&fake_mask==0U);
    fake_mask=1;fake_tim.CNT=7U;
    CHECK(RuntimeClock_Read()==UINT32_MAX+8ULL&&fake_mask==1U);
    puts("PASS: TIM5 1MHz, 32->64 bit wrap, interrupt mask preserved");
    const char *names[]={"GUI","Camera","IDLE"};
    for(unsigned i=0;i<3;++i)tasks[i]=(TaskStatus_t){.xHandle=(void *)(uintptr_t)(i+1U),
        .pcTaskName=names[i],.xTaskNumber=i+1U,.uxCurrentPriority=2U,
        .pxStackBase=stacks[i],.pxTopOfStack=&stacks[i][200],.pxEndOfStack=&stacks[i][255],
        .usStackHighWaterMark=10U};
    const SystemMonitorSnapshot *s=sample(1000,4300000000ULL);
    /* 首屏已有真实累计值，不等待5秒；后续相邻1秒数据更新才是当前窗口值。 */
    CHECK(s->complete&&s->count==3&&s->cpu_valid&&s->cpu_permille==450U&&s->scheduler_running);
    CHECK(s->tasks[0].used_bytes==120U&&s->tasks[0].stack_bytes==1024U);
    CHECK(s->tasks[1].used_bytes==224U&&s->tasks[0].min_free_bytes==40U);
    CHECK(s->heap_free==10000U&&s->largest_heap_block==9000U);
    s=sample(2000,4301000000ULL);
    CHECK(s->cpu_valid&&s->cpu_permille==450U&&s->cpu_window_ms==1000U);
    CHECK(s->tasks[0].cpu_valid&&s->tasks[0].cpu_permille==350U);
    CHECK(s->tasks[1].cpu_permille==100U&&s->tasks[2].cpu_permille==550U);
    /* 将下一秒的负载改为CPU80%/空闲20%，确认数值一秒就变化，非缓存5秒结果。 */
    ms=3000;ticks+=1000000ULL;fake_tim.CNT=(uint32_t)ticks;
    tasks[0].ulRunTimeCounter+=700000ULL;tasks[1].ulRunTimeCounter+=100000ULL;
    tasks[2].ulRunTimeCounter+=200000ULL;s=SystemMonitor_Read(NULL);
    CHECK(s->cpu_permille==800U&&s->tasks[0].cpu_permille==700U&&s->cpu_window_ms==1000U);
    unsigned before=reads,idle_before=idle_reads;
    ms=3500;SystemMonitor_Poll();SystemMonitor_Read(NULL);CHECK(reads==before&&idle_reads==idle_before);
    tasks[1].xTaskNumber=99U;s=sample(4000,4303000000ULL);
    CHECK(!s->tasks[1].cpu_valid); /* 同句柄新任务不能继承已删除任务的窗口结果。 */
    tasks[1].pxTopOfStack=stacks[1]-0;tasks[1].pxEndOfStack=stacks[1];
    s=sample(5000,4304000000ULL);CHECK(s->tasks[1].stack_bytes==4U);
    tasks[1].pxTopOfStack=&stacks[1][20];s=sample(6000,4305000000ULL);
    CHECK(!s->tasks[1].current_valid);
    total_tasks=13;s=sample(7000,4306000000ULL);
    CHECK(!s->complete&&s->cpu_valid&&s->count==0U); /* 名单容量不足不影响独立CPU计数。 */
    total_tasks=3;s=sample(8000,4307000000ULL);CHECK(s->complete&&s->cpu_valid);
    /* 模拟离开页面：只轮询轻量计数10秒，禁止调用任务/栈扫描接口。 */
    before=reads;
    for(unsigned i=0;i<10;++i) {
        ms+=1000U;ticks+=1000000ULL;fake_tim.CNT=(uint32_t)ticks;
        tasks[2].ulRunTimeCounter+=900000ULL;SystemMonitor_Poll();
    }
    CHECK(reads==before);s=SystemMonitor_Read(NULL);
    CHECK(s->cpu_valid&&s->cpu_permille==100U&&s->cpu_window_ms==1000U);
    scheduler_running=false;ms+=1000U;fake_tim.CNT+=1000000U;s=SystemMonitor_Read(NULL);
    CHECK(!s->scheduler_running&&!s->cpu_valid);
    puts("PASS: immediate CPU, 1s changing load, hidden-page lightweight polling, reopen freshness, scheduler state, reordered tasks, PSP, identity reuse, capacity");
    return 0;
}
