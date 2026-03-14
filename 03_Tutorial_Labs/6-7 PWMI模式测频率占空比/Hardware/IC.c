#include "stm32f10x.h"                  // Device header

void IC_Init(void)
{
	// 1. 使能外设时钟（时钟是外设工作的前提，必须先开启）
    // TIM2挂载在APB1总线，所以用RCC_APB1PeriphClockCmd开启时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    // PA0是TIM2通道1的默认输出引脚，GPIOA挂载在APB2总线，开启GPIOA时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    // 2. 配置GPIO引脚（PA0）为PWM输出模式
    GPIO_InitTypeDef GPIO_InitStructure;  // GPIO初始化结构体（存储GPIO配置参数）
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;   //上拉输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;       // 配置PA6引脚（已修改）
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; // GPIO响应速度：50MHz（常用配置，匹配定时器时钟）
    GPIO_Init(GPIOA, &GPIO_InitStructure);  // 将配置参数写入GPIOA寄存器，完成初始化
    
    // 3. 配置定时器时钟源（选择内部时钟）
    TIM_InternalClockConfig(TIM3);  // TIM2使用内部时钟（来自APB1总线时钟，默认72MHz，与系统时钟一致）
                                    // 也可选择外部时钟（TIM_ITRxExternalClockConfig），此处用内部时钟更简单稳定
    
    // 4. 配置定时器时基单元（决定PWM频率的核心配置）
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;  // 时基初始化结构体
    TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;  // 时钟分频系数：不分频（CKD_DIV1）
                                                            // 作用是滤波定时器时钟，此处无需滤波，直接用原始时钟
    TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;  // 计数模式：向上计数（从0计数到ARR，循环）
    TIM_TimeBaseInitStruct.TIM_Period = 65536 - 1;    // 设置大一些防止溢出
                                                    // 关联PWM频率：65536+1 = 65536（计数周期长度）
    TIM_TimeBaseInitStruct.TIM_Prescaler = 72 - 1;  // 预分频器（PSC）：71（将定时器时钟分频为 72MHz/(72) = 1MHz）
                                                    // 计数时钟CK_CNT = 系统时钟 / (PSC+1) = 72MHz/72 = 1MHz
    TIM_TimeBaseInitStruct.TIM_RepetitionCounter = 0;  // 重复计数器：0（仅高级定时器用，通用定时器TIM2无此功能）
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStruct);  // 将配置写入TIM3时基寄存器，完成时基初始化
	
	//5.初始化时基单元
	TIM_ICInitTypeDef TIM_ICInitStructure;
	TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
	TIM_ICInitStructure.TIM_ICFilter = 0xF;
	TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
	TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
	TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
	
	TIM_PWMIConfig(TIM3, &TIM_ICInitStructure);
	
	//6.配置TRGI的触发源为TI1FP1,配置从模式为Reset,启动定时器
	TIM_SelectInputTrigger(TIM3, TIM_TS_TI1FP1);
	TIM_SelectSlaveMode(TIM3, TIM_SlaveMode_Reset);
	
	TIM_Cmd(TIM3, ENABLE);
}

uint32_t IC_GetFreq(void)//返回最新一个周期频率值，单位周期
{
	return 1000000 / (TIM_GetCapture1(TIM3) + 1);
}

uint32_t IC_GetDuty(void)
{
	return (TIM_GetCapture2(TIM3) + 1) * 100 / (TIM_GetCapture1(TIM3) + 1);
}
