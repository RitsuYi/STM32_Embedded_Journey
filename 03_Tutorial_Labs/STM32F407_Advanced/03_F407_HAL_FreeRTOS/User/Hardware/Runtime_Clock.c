#include "Runtime_Clock.h"
#include "main.h"
#include <stdbool.h>

static uint32_t previous;
static uint64_t elapsed;
static bool clock_ready;

void RuntimeClock_Init(void)
{
    /* APB1有分频时，F407定时器时钟等于PCLK1的两倍：当前42MHz×2=84MHz。
     * 84分频得到1MHz，每个计数代表1微秒，精度比1ms系统节拍更适合统计短任务。 */
    uint32_t hz=HAL_RCC_GetPCLK1Freq();
    if((RCC->CFGR&RCC_CFGR_PPRE1)!=0U)hz*=2U;
    if(hz<1000000U || hz%1000000U!=0U)Error_Handler();
    __HAL_RCC_TIM5_CLK_ENABLE();
    __HAL_RCC_TIM5_FORCE_RESET();
    __HAL_RCC_TIM5_RELEASE_RESET();
    TIM5->PSC=hz/1000000U-1U;
    TIM5->ARR=UINT32_MAX;
    TIM5->EGR=TIM_EGR_UG;
    TIM5->SR=0U;
    TIM5->CNT=0U;
    /* 调试暂停时连统计计时一起冻结，避免暂停一分钟后错误地显示CPU空闲一分钟。 */
    __HAL_DBGMCU_FREEZE_TIM5();
    previous=0U;elapsed=0U;clock_ready=true;
    TIM5->CR1=TIM_CR1_CEN;
}

uint64_t RuntimeClock_Read(void)
{
    /* TIM5的32位硬件计数约71.6分钟回绕。任务切换频繁读取，用无符号差值累加到
     * 64位后可跨越回绕；要求两次读取间隔少于一次硬件回绕，当前500ms LED定时任务
     * 已保证这一点。短暂保留/恢复PRIMASK，避免读64位时被另一次任务切换打断。 */
    uint32_t mask=__get_PRIMASK();__disable_irq();
    uint32_t now=clock_ready?TIM5->CNT:0U;
    elapsed+=(uint32_t)(now-previous);previous=now;
    uint64_t value=elapsed;
    __set_PRIMASK(mask);
    return value;
}
