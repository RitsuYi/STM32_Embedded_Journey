#include "stm32f10x.h"                  // STM32F10x系列芯片的设备头文件
// 包含所有寄存器定义、外设初始化函数声明、宏定义等，是STM32F1开发的基础头文件

/**
  * @brief  TIM2通道1的PWM输出初始化函数
  * @note   配置PA0为PWM输出口，生成指定频率和占空比的PWM波
  *         核心依赖：定时器时基单元（PSC/ARR）决定频率，捕获比较单元（CCR）决定占空比
  * @param  无
  * @retval 无
  */
void PWM_Init(void)
{
    // 1. 使能外设时钟（时钟是外设工作的前提，必须先开启）
    // TIM2挂载在APB1总线，所以用RCC_APB1PeriphClockCmd开启时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    // PA0是TIM2通道1的默认输出引脚，GPIOA挂载在APB2总线，开启GPIOA时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    // 【可选代码：引脚重映射配置】（当前代码注释未启用，仅作说明）
    // RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);  // 使能AFIO复用功能时钟（重映射必须开启）
    // GPIO_PinRemapConfig(GPIO_PartialRemap1_TIM2, ENABLE); // TIM2部分重映射：通道1从PA0映射到PA15
    // GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE); // 关闭JTAG功能，释放PA15引脚（PA15默认是JTAG引脚）
    
    // 2. 配置GPIO引脚（PA0）为PWM输出模式
    GPIO_InitTypeDef GPIO_InitStructure;  // GPIO初始化结构体（存储GPIO配置参数）
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  // 模式：复用推挽输出（PWM是定时器复用功能，必须用AF_PP）
                                                    // 推挽输出：高低电平驱动能力强；复用：引脚功能由定时器控制，而非普通IO
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;       // 配置PA0引脚（TIM2_CH1默认引脚）
    // GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;    // 若启用重映射，需改为PA15引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; // GPIO响应速度：50MHz（常用配置，匹配定时器时钟）
    GPIO_Init(GPIOA, &GPIO_InitStructure);  // 将配置参数写入GPIOA寄存器，完成初始化
    
    // 3. 配置定时器时钟源（选择内部时钟）
    TIM_InternalClockConfig(TIM2);  // TIM2使用内部时钟（来自APB1总线时钟，默认72MHz，与系统时钟一致）
                                    // 也可选择外部时钟（TIM_ITRxExternalClockConfig），此处用内部时钟更简单稳定
    
    // 4. 配置定时器时基单元（决定PWM频率的核心配置）
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;  // 时基初始化结构体
    TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;  // 时钟分频系数：不分频（CKD_DIV1）
                                                            // 作用是滤波定时器时钟，此处无需滤波，直接用原始时钟
    TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;  // 计数模式：向上计数（从0计数到ARR，循环）
    TIM_TimeBaseInitStruct.TIM_Period = 100 - 1;    // 自动重装载值（ARR）：99（计数到99后清零，周期为ARR+1个计数时钟）
                                                    // 关联PWM频率：ARR+1 = 100（计数周期长度）
    TIM_TimeBaseInitStruct.TIM_Prescaler = 720 - 1;  // 预分频器（PSC）：719（将定时器时钟分频为 72MHz/(720) = 100kHz）
                                                    // 计数时钟CK_CNT = 系统时钟 / (PSC+1) = 72MHz/720 = 100kHz
    TIM_TimeBaseInitStruct.TIM_RepetitionCounter = 0;  // 重复计数器：0（仅高级定时器用，通用定时器TIM2无此功能）
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStruct);  // 将配置写入TIM2时基寄存器，完成时基初始化
	
    // 5. 配置PWM输出通道（通道1，决定占空比的核心配置）
    TIM_OCInitTypeDef TIM_OCInitStruct;  // 输出比较（OC）初始化结构体（PWM基于输出比较功能实现）
    TIM_OCStructInit(&TIM_OCInitStruct); // 初始化OC结构体为默认值（避免未赋值参数导致配置异常）
    TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;  // PWM模式1：计数器值 < CCR时，输出有效电平；>= CCR时，输出无效电平
                                                   // 模式2则相反，此处用模式1是最常用的PWM模式
    TIM_OCInitStruct.TIM_OCPolarity = TIM_OCNPolarity_High;  // 输出极性：高电平有效（即PWM的高电平为有效电平）
                                                           // 若设为Low，则有效电平为低电平
    TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;  // 输出使能：开启通道1的PWM输出
    TIM_OCInitStruct.TIM_Pulse = 0;  // 捕获比较值（CCR）：初始值0（此时占空比 = 0/(ARR+1) = 0%）
                                    // 后续可通过PWM_SetCompare1函数修改此值，改变占空比
    TIM_OC1Init(TIM2, &TIM_OCInitStruct);  // 将配置写入TIM2通道1的OC寄存器，完成PWM通道配置
	
    // 6. 使能定时器（定时器开始计数，才能生成PWM波）
    TIM_Cmd(TIM2, ENABLE);  // 开启TIM2定时器，计数器开始从0向上计数，PWM波开始输出
}

/**
  * @brief  修改TIM2通道1的PWM占空比
  * @note   通过修改捕获比较寄存器CCR1的值，改变PWM高电平占比
  * @param  Compare：新的捕获比较值（CCR1），范围0 ~ ARR（即0~99）
  *         占空比计算公式：占空比 = (Compare / (ARR+1)) * 100%
  * @retval 无
  */
void PWM_SetCompare1(uint16_t Compare)
{
    // 直接写入CCR1寄存器，修改捕获比较值（无需重新初始化，实时生效）
    TIM_SetCompare1(TIM2, Compare);
}

void PWM_SetPrescaler(uint16_t Prescaler)
{
	TIM_PrescalerConfig(TIM2, Prescaler, TIM_PSCReloadMode_Immediate);
}
