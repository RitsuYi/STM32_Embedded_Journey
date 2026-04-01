#include "stm32f10x.h"                  // Device header

// 全局变量：存储编码器计数结果（int16_t支持正负值，对应正反转）
int16_t Encoder_Count;

/**
  * @brief  编码器初始化（基于外部中断实现）
  * @param  无
  * @retval 无
  */
void Encoder_Init(void)
{
	// 1. 使能外设时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);        // 使能GPIOB时钟（对应编码器引脚PB0、PB1）
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);         // 使能AFIO时钟（外部中断必须依赖AFIO进行引脚映射，不可省略）
	
	// 2. 配置GPIO引脚（PB0、PB1）为上拉输入
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;                // 上拉输入：编码器未触发时引脚为高电平，避免杂波触发误中断
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;       // 配置PB0、PB1引脚（对应编码器A、B两相）
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;            // 输入模式下此参数无实际作用，默认配置50MHz即可
	GPIO_Init(GPIOB, &GPIO_InitStructure);                       // 应用配置到GPIOB，完成GPIO初始化
	
	// 3. 配置GPIO引脚与外部中断线路的映射关系
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource0);  // 将PB0映射到EXTI_Line0中断线路
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource1);  // 将PB1映射到EXTI_Line1中断线路
	
	// 4. 初始化外部中断（EXTI）配置
	EXTI_InitTypeDef EXTI_InitStructure;
	EXTI_InitStructure.EXTI_Line = EXTI_Line0 | EXTI_Line1;      // 选择要配置的中断线路：Line0（对应PB0）、Line1（对应PB1）
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;                    // 使能该中断线路
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;          // 中断模式（而非事件模式），触发后进入中断服务函数
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;      // 下降沿触发中断（对应编码器引脚从高电平变低电平）
	EXTI_Init(&EXTI_InitStructure);                              // 应用配置到EXTI，完成外部中断初始化
	
	// 5. 配置NVIC（嵌套向量中断控制器），使能中断并设置优先级
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);              // 设置NVIC优先级分组2：2位抢占优先级，2位子优先级（整个系统只能配置一次）
	
	NVIC_InitTypeDef NVIC_Initstructure;
	
	// 配置EXTI0中断（对应PB0）
	NVIC_Initstructure.NVIC_IRQChannel = EXTI0_IRQn;             // 选择中断通道：EXTI0中断
	NVIC_Initstructure.NVIC_IRQChannelCmd = ENABLE;              // 使能该中断通道
	NVIC_Initstructure.NVIC_IRQChannelPreemptionPriority = 1;    // 抢占优先级：1（数值越小，优先级越高）
	NVIC_Initstructure.NVIC_IRQChannelSubPriority = 1;           // 子优先级：1（抢占优先级相同时，比较子优先级）
	NVIC_Init(&NVIC_Initstructure);                              // 应用配置到NVIC，完成EXTI0中断配置
	
	// 配置EXTI1中断（对应PB1）
	NVIC_Initstructure.NVIC_IRQChannel = EXTI1_IRQn;             // 选择中断通道：EXTI1中断
	NVIC_Initstructure.NVIC_IRQChannelCmd = ENABLE;              // 使能该中断通道
	NVIC_Initstructure.NVIC_IRQChannelPreemptionPriority = 1;    // 抢占优先级：1（与EXTI0相同）
	NVIC_Initstructure.NVIC_IRQChannelSubPriority = 2;           // 子优先级：2（低于EXTI0，避免中断冲突）
	NVIC_Init(&NVIC_Initstructure);                              // 应用配置到NVIC，完成EXTI1中断配置
}

/**
  * @brief  获取编码器单次计数增量并清零全局计数
  * @param  无
  * @retval 本次获取的计数增量（正数：正转，负数：反转，0：无转动）
  */
int16_t Encoder_Get(void)
{
	int16_t Temp;
	Temp = Encoder_Count;    // 暂存当前全局计数结果（避免读取过程中计数被中断修改）
	Encoder_Count = 0;       // 清零全局计数，为下一次计数做准备
	
	return Temp;             // 返回本次计数增量
}

/**
  * @brief  EXTI0中断服务函数（对应PB0下降沿触发）
  * @param  无
  * @retval 无
  */
void EXTI0_IRQHandler(void)
{
	// 判断是否为EXTI_Line0（PB0）触发的中断（避免误触发）
	if(EXTI_GetITStatus(EXTI_Line0) == SET)
	{
		// 正交编码相位判断：读取PB1电平，确认编码器正转
		// （PB0下降沿时，PB1也为低电平，判定为正转，计数加1）
		if(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == 0)
		{
			Encoder_Count++;		
		}
		EXTI_ClearITPendingBit(EXTI_Line0);  // 清除EXTI_Line0中断挂起位（必须执行，否则会重复触发中断）
	}
}

/**
  * @brief  EXTI1中断服务函数（对应PB1下降沿触发）
  * @param  无
  * @retval 无
  */
void EXTI1_IRQHandler(void)
{
	// 判断是否为EXTI_Line1（PB1）触发的中断（避免误触发）
	if(EXTI_GetITStatus(EXTI_Line1) == SET)
	{
		// 正交编码相位判断：读取PB0电平，确认编码器反转
		// （PB1下降沿时，PB0也为低电平，判定为反转，计数减1）
		if(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0) == 0)
		{
			Encoder_Count--;		
		}
		EXTI_ClearITPendingBit(EXTI_Line1);  // 清除EXTI_Line1中断挂起位（必须执行，否则会重复触发中断）
	}
}