#include "stm32f10x.h"                  // Device header

// 声明外部全局变量Num：用于在定时器中断服务函数中进行计数（定义在其他文件中）
extern uint16_t Num;

/**
  * @brief  定时器TIM2初始化（定时中断模式，基于内部时钟）
  * @param  无
  * @retval 无
  */
void Timer_Init(void)
{
	// 1. 使能TIM2外设时钟（TIM2挂载在APB1总线上，必须先开启时钟才能配置外设）
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	
	// 2. 配置TIM2时钟源为内部时钟（默认时钟源，来自APB1总线，频率72MHz，稳定可靠）
	TIM_InternalClockConfig(TIM2);
	
	// 3. 初始化定时器时基单元（决定定时时长的核心配置）
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
	TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;          // 时钟分频系数：不分频（仅用于滤波定时器时钟，此处无需滤波）
	TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;      // 计数模式：向上计数（从0计数到ARR值，溢出后触发更新中断）
	TIM_TimeBaseInitStruct.TIM_Period = 5000 - 1;                    // 自动重装值（ARR）：4999
	                                                                  // 计数到10000次时溢出，配合预分频器决定定时时长
	TIM_TimeBaseInitStruct.TIM_Prescaler = 7200 - 1;                  // 预分频器（PSC）：7199
	                                                                  // 定时器计数时钟 = 72MHz / (7200) = 10kHz（每100us计数1次）
	                                                                  // 总定时时长 = (ARR+1) * (PSC+1) / 72MHz = 5000*7200/72MHz = 0.5s
	TIM_TimeBaseInitStruct.TIM_RepetitionCounter = 0;                 // 重复计数器：0（仅高级定时器有效，通用定时器TIM2无此功能，设为0即可）
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStruct);                  // 应用配置到TIM2，完成时基单元初始化
	
	// 4. 清除TIM2更新中断标志位（防止定时器初始化完成后，立即触发一次虚假更新中断）
	TIM_ClearFlag(TIM2, TIM_FLAG_Update);
	
	// 5. 使能TIM2更新中断（允许定时器溢出时，触发中断请求）
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
	
	// 6. 配置NVIC（嵌套向量中断控制器），使能TIM2中断并设置优先级
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);              // 设置NVIC优先级分组2：2位抢占优先级，2位子优先级（整个系统仅需配置一次）
	
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;              // 选择中断通道：TIM2中断（对应定时器更新中断）
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;              // 使能该中断通道
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;    // 抢占优先级：2（数值越小，优先级越高）
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;           // 子优先级：1（抢占优先级相同时，比较子优先级）
	NVIC_Init(&NVIC_InitStructure);                              // 应用配置到NVIC，完成TIM2中断优先级配置
	
	// 7. 开启TIM2定时器（定时器开始按照时基配置计数，计数溢出后触发中断）
	TIM_Cmd(TIM2, ENABLE);
}

/*
* @brief  TIM2中断服务函数（处理定时器更新中断，即定时时间到的回调）
*/
/*
void TIM2_IRQHandler(void)
{
	// 判断是否为TIM2更新中断触发（避免误触发其他中断）
	if(TIM_GetITStatus(TIM2, TIM_IT_Update) == SET)
	{
		// 此处可添加定时时间到后的业务逻辑（例如：Num++，实现秒计数）
		// Num++;
		
		// 清除TIM2更新中断挂起位（必须执行，否则会重复触发该中断）
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	}
}
*/
