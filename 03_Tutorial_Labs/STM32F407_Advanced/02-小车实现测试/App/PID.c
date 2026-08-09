#include "PID.h"
#include "BoardConfig.h"

/* 速度环 PID 的全局配置。
 * 这里保存的是“当前生效的一组默认参数”，外部可以先修改这组参数，
 * 再通过 PID_ApplySpeedConfig() 批量下发到某个具体 PID 实例。 */
static PID_SpeedConfig_t g_speedPidConfig;

/* 取浮点数绝对值。
 * 本模块中大量限制参数都要求是“幅值”概念，因此统一先转成正数再使用，
 * 可以避免外部误传负值后导致上下限逻辑混乱。 */
static float PID_Abs(float value)
{
	if (value < 0.0f)
	{
		return -value;
	}

	return value;
}

/* 将 value 限制在 [minValue, maxValue] 范围内。
 * 该函数主要用于：
 * 1. 限制积分项，防止积分饱和过大；
 * 2. 限制最终输出，避免 PWM 或控制量超出允许范围。 */
static float PID_Clamp(float value, float minValue, float maxValue)
{
	if (value > maxValue)
	{
		return maxValue;
	}

	if (value < minValue)
	{
		return minValue;
	}

	return value;
}

/* 获取安全的输出限幅值。
 * 输出限幅本质上是一个“最大幅度”，所以先取绝对值；
 * 同时把最小值保护到 1.0，避免外部把限幅配置为 0 或过小后，
 * 导致 PID 输出几乎被完全锁死。 */
static float PID_GetSafeOutputLimit(float outputLimit)
{
	outputLimit = PID_Abs(outputLimit);
	if (outputLimit < 1.0f)
	{
		outputLimit = 1.0f;
	}

	return outputLimit;
}

/* 从板级配置中加载一组默认的速度环 PID 参数。
 * 这一步只是更新全局配置，并不会自动写入某个 PID 实例；
 * 如需让具体控制器使用这组参数，还需要再调用 PID_ApplySpeedConfig()。 */
void PID_LoadSpeedConfig(void)
{
	g_speedPidConfig.kp = BOARD_SPEED_PID_KP;
	g_speedPidConfig.ki = BOARD_SPEED_PID_KI;
	g_speedPidConfig.kd = BOARD_SPEED_PID_KD;
	g_speedPidConfig.integralLimit = PID_Abs(BOARD_SPEED_PID_INTEGRAL_LIMIT);
	g_speedPidConfig.outputLimit = PID_GetSafeOutputLimit((float)BOARD_SPEED_PID_OUTPUT_LIMIT);
	g_speedPidConfig.outputOffset = 0.0f;
}

/* 读取当前全局速度环配置。
 * 使用“结构体整体拷贝”的方式导出，便于上层调试、显示或通过串口回传。 */
void PID_GetSpeedConfig(PID_SpeedConfig_t *config)
{
	if (config == 0)
	{
		/* 传入空指针时直接返回，避免非法访问。 */
		return;
	}

	*config = g_speedPidConfig;
}

/* 动态修改速度环 PID 三个核心系数。
 * 这里只更新全局配置，不直接修改某个 PID 实例。 */
void PID_SetSpeedTunings(float kp, float ki, float kd)
{
	g_speedPidConfig.kp = kp;
	g_speedPidConfig.ki = ki;
	g_speedPidConfig.kd = kd;
}

/* 动态修改积分限幅和总输出限幅。
 * 外部即使传入负数，也会在内部转换成“幅值”的概念。 */
void PID_SetSpeedLimits(float integralLimit, float outputLimit)
{
	g_speedPidConfig.integralLimit = PID_Abs(integralLimit);
	g_speedPidConfig.outputLimit = PID_GetSafeOutputLimit(outputLimit);
}

/* 初始化一个 PID 实例。
 * 这里会把状态量、参数和限幅全部清零，适合在对象第一次使用时调用。
 * 注意：初始化后 Kp/Ki/Kd 仍然是 0，通常还需要再调用
 * PID_ApplySpeedConfig() 或由上层手动赋值，PID 才会真正产生控制作用。 */
void PID_Init(PID_t *p)
{
	if (p == 0)
	{
		/* 防御式编程：空指针不做任何操作。 */
		return;
	}

	/* 目标值、当前值、上一拍实际值。 */
	p->Target = 0.0f;
	p->Actual = 0.0f;
	p->Actual1 = 0.0f;

	/* 最终输出。 */
	p->Out = 0.0f;

	/* PID 三个系数。 */
	p->Kp = 0.0f;
	p->Ki = 0.0f;
	p->Kd = 0.0f;

	/* 当前误差与上一拍误差。 */
	p->Error0 = 0.0f;
	p->Error1 = 0.0f;

	/* 比例、积分、微分三部分输出。 */
	p->POut = 0.0f;
	p->IOut = 0.0f;
	p->DOut = 0.0f;

	/* 积分限幅。 */
	p->IOutMax = 0.0f;
	p->IOutMin = 0.0f;

	/* 总输出限幅。 */
	p->OutMax = 0.0f;
	p->OutMin = 0.0f;

	/* 输出偏置。
	 * 常用于电机克服死区：当输出不为 0 时，额外补一点基础驱动力。 */
	p->OutOffset = 0.0f;
}

/* 重置 PID 的运行状态。
 * 与 PID_Init() 不同，这里不会清除 Kp/Ki/Kd 和各种限幅参数，
 * 适合在切换模式、重新起步或希望清空历史积分时调用。 */
void PID_Reset(PID_t *p)
{
	if (p == 0)
	{
		return;
	}

	/* 清空运行过程中的目标、反馈、误差和输出状态。 */
	p->Target = 0.0f;
	p->Actual = 0.0f;
	p->Actual1 = 0.0f;
	p->Out = 0.0f;
	p->Error0 = 0.0f;
	p->Error1 = 0.0f;
	p->POut = 0.0f;
	p->IOut = 0.0f;
	p->DOut = 0.0f;
}

/* 将全局速度环配置应用到某个具体 PID 实例。
 * 这样做的好处是：多个电机 PID 可以共享同一套默认参数，
 * 但每个 PID 又独立保存自己的积分状态和历史状态。 */
void PID_ApplySpeedConfig(PID_t *p)
{
	float integralLimit;
	float outputLimit;

	if (p == 0)
	{
		return;
	}

	/* 再次做一次归一化处理，保证内部上下限一定合法。 */
	integralLimit = PID_Abs(g_speedPidConfig.integralLimit);
	outputLimit = PID_GetSafeOutputLimit(g_speedPidConfig.outputLimit);

	/* 下发 PID 系数。 */
	p->Kp = g_speedPidConfig.kp;
	p->Ki = g_speedPidConfig.ki;
	p->Kd = g_speedPidConfig.kd;

	/* 将“幅值”转换成真正的上下限。 */
	p->IOutMax = integralLimit;
	p->IOutMin = -integralLimit;
	p->OutMax = outputLimit;
	p->OutMin = -outputLimit;

	/* 输出偏置统一按正值保存，实际方向在更新输出时再决定加还是减。 */
	p->OutOffset = PID_Abs(g_speedPidConfig.outputOffset);

	/* 如果应用新参数前 PID 已经在运行，旧积分值/旧输出可能超出新限幅，
	 * 这里立即夹紧，避免参数切换后出现突变。 */
	p->IOut = PID_Clamp(p->IOut, p->IOutMin, p->IOutMax);
	p->Out = PID_Clamp(p->Out, p->OutMin, p->OutMax);
}

/* 执行一次 PID 更新。
 * 当前实现特点：
 * 1. 比例项基于当前误差；
 * 2. 积分项采用误差累加，并带积分限幅；
 * 3. 微分项不是对误差求导，而是对实际值变化量求导，
 *    可避免目标值突变时产生明显的“微分冲击”；
 * 4. 最终输出支持死区补偿偏置，并带总输出限幅。 */
void PID_Update(PID_t *p)
{
	if (p == 0)
	{
		return;
	}

	/* 保存上一拍误差，并计算当前误差。 */
	p->Error1 = p->Error0;
	p->Error0 = p->Target - p->Actual;

	/* 比例项：误差越大，立刻输出越大。 */
	p->POut = p->Kp * p->Error0;

	if (p->Ki != 0.0f)
	{
		/* 积分项：持续误差会不断累加，用于消除稳态误差。 */
		p->IOut += p->Ki * p->Error0;
	}
	else
	{
		/* Ki 为 0 时，主动清零积分项，避免保留历史积分造成误导。 */
		p->IOut = 0.0f;
	}

	/* 对积分项做限幅，抑制积分饱和。 */
	p->IOut = PID_Clamp(p->IOut, p->IOutMin, p->IOutMax);

	/* 微分项：使用“测量值微分”写法。
	 * 若实际值上涨过快，则给出负向制动；若下降过快，则给出正向补偿。 */
	p->DOut = -p->Kd * (p->Actual - p->Actual1);

	/* PID 三项求和得到基础输出。 */
	p->Out = p->POut + p->IOut + p->DOut;

	if (p->Out > 0.0f)
	{
		/* 正向输出时叠加正向偏置，用于克服电机死区。 */
		p->Out += p->OutOffset;
	}
	else if (p->Out < 0.0f)
	{
		/* 反向输出时叠加反向偏置，保证前后方向死区补偿一致。 */
		p->Out -= p->OutOffset;
	}

	/* 最终输出限幅，防止超过执行器允许范围。 */
	p->Out = PID_Clamp(p->Out, p->OutMin, p->OutMax);

	/* 记录本次实际值，供下一次计算微分项使用。 */
	p->Actual1 = p->Actual;
}

/* 计算接口的便捷封装。
 * 外部只需要给出目标值和实际值，即可完成一次完整 PID 运算，
 * 返回值就是本次控制输出。 */
float PID_Calculate(PID_t *p, float target, float actual)
{
	if (p == 0)
	{
		return 0.0f;
	}

	/* 先更新输入，再执行一次 PID 运算。 */
	p->Target = target;
	p->Actual = actual;
	PID_Update(p);
	return p->Out;
}
