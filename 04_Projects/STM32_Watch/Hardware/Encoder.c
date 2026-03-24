#include "stm32f10x.h"                  // Device header

void Encoder_Init(void)
{
	// 1. 使能外设时钟（时钟是外设工作的前提，必须先开启）
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // 2. 配置GPIO引脚（PB0, PB10）为编码器输入模式
    GPIO_InitTypeDef GPIO_InitStructure;                               // GPIO初始化结构体（存储GPIO配置参数）
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;                      //上拉输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_10;            // 配置PB0, PB10引脚（编码器A/B相）
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;                  // GPIO响应速度：50MHz（常用配置，匹配定时器时钟）
    GPIO_Init(GPIOB, &GPIO_InitStructure);                             // 将配置参数写入GPIOB寄存器，完成初始化
    
    // 3. 配置定时器时钟源（选择内部时钟）编码器接口会托管时钟
    //TIM_InternalClockConfig(TIM3);                                   // TIM2使用内部时钟（来自APB1总线时钟，默认72MHz，与系统时钟一致）
                                                                       // 也可选择外部时钟（TIM_ITRxExternalClockConfig），此处用内部时钟更简单稳定
    
    // 4. 配置定时器时基单元（决定PWM频率的核心配置）
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;                    // 时基初始化结构体
    TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;           // 时钟分频系数：不分频（CKD_DIV1）
                                                                       // 作用是滤波定时器时钟，此处无需滤波，直接用原始时钟
    TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;       // 计数模式：向上计数（从0计数到ARR，循环）        无用
    TIM_TimeBaseInitStruct.TIM_Period = 65536 - 1;                     // 设置大一些防止溢出
                                                                       // 关联PWM频率：65536+1 = 65536（计数周期长度）
    TIM_TimeBaseInitStruct.TIM_Prescaler = 1 - 1;                      // 预分频器（PSC）：1
    TIM_TimeBaseInitStruct.TIM_RepetitionCounter = 0;                  // 重复计数器：0（仅高级定时器用，通用定时器TIM2无此功能）
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStruct);                   // 将配置写入TIM3时基寄存器，完成时基初始化
	
	//5.初始化时基单元
	TIM_ICInitTypeDef TIM_ICInitStructure;
	TIM_ICStructInit(&TIM_ICInitStructure);                            // 结构体填写未完全 防止出现问题要先初始化给所有变量赋值
	TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;                   // 选择要配置的定时器通道：通道1
	TIM_ICInitStructure.TIM_ICFilter = 0xF;                            // 捕获滤波器参数：0xF为最大滤波强度，用于抑制输入信号上的高频噪声
	//TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;      // 极性选择：选择上升沿触发捕获 TI1 TI2是否反向
	//TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;            // 捕获预分频：不分频，每检测到1个有效触发事件就执行1次捕获 
	//TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;  // 捕获信号选择：直接将通道1的输入引脚（TI1）作为捕获输入源  
	
	TIM_ICInit(TIM3, &TIM_ICInitStructure);                            // 将上述配置参数应用到TIM3定时器，完成输入捕获通道1的初始化 
	
	TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;                   // 选择要配置的定时器通道：通道2
	TIM_ICInitStructure.TIM_ICFilter = 0xF;                            // 捕获滤波器参数：0xF为最大滤波强度，用于抑制输入信号上的高频噪声
	//TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;      // 极性选择：选择上升沿触发捕获 TI1 TI2是否反向
	//TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;            // 捕获预分频：不分频，每检测到1个有效触发事件就执行1次捕获 
	//TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;  // 捕获信号选择：直接将通道1的输入引脚（TI1）作为捕获输入源  
	
	TIM_ICInit(TIM3, &TIM_ICInitStructure);                            // 将上述配置参数应用到TIM3定时器，完成输入捕获通道1的初始化 
	
	//6.配置编码器 后两个参数和前面的极性选择一样 这个要在IC_Init下面
	TIM_EncoderInterfaceConfig(TIM3, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);
	
	//7.开启定时器
	TIM_Cmd(TIM3, ENABLE);
	
}

int16_t Encoder_Get(void)
{
	int16_t Temp;
	Temp = TIM_GetCounter(TIM3);
	TIM_SetCounter(TIM3, 0);
	return Temp;
}
