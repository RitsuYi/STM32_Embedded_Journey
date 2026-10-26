#ifndef SYSTEM_TEST_MAIN_H
#define SYSTEM_TEST_MAIN_H
#include <stdint.h>
#include <stddef.h>
/* 用寄存器模型验证真实计时器代码的分频/回绕，不代表已接到F407。 */
typedef struct {uint32_t CFGR;} FakeRCC;
typedef struct {uint32_t PSC,ARR,EGR,SR,CNT,CR1;} FakeTIM;
extern FakeRCC fake_rcc;extern FakeTIM fake_tim;
#define RCC (&fake_rcc)
#define TIM5 (&fake_tim)
#define RCC_CFGR_PPRE1 0x1C00U
#define TIM_EGR_UG 1U
#define TIM_CR1_CEN 1U
extern unsigned fake_frozen,fake_mask;
#define __HAL_RCC_TIM5_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM5_FORCE_RESET() ((void)0)
#define __HAL_RCC_TIM5_RELEASE_RESET() ((void)0)
#define __HAL_DBGMCU_FREEZE_TIM5() (++fake_frozen)
uint32_t HAL_GetTick(void);
uint32_t HAL_RCC_GetPCLK1Freq(void);
uintptr_t __get_PSP(void);
static inline uint32_t __get_PRIMASK(void){return fake_mask;}
static inline void __disable_irq(void){fake_mask=1;}
static inline void __set_PRIMASK(uint32_t mask){fake_mask=mask;}
void Error_Handler(void);
#endif
