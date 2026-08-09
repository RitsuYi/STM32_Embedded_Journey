#ifndef __PID_H
#define __PID_H

#include "stm32f4xx.h"

/*
 * PID 控制器的完整运行状态。
 *
 * 可以把 PID_t 理解成一个控制器的“记事本”：它既保存 Kp、Ki、Kd 等参数，
 * 也保存上一轮误差、累计积分等历史数据。每个电机都应拥有独立的 PID_t，
 * 否则不同电机的历史数据会互相干扰。
 */
typedef struct
{
	float Target;       /* 目标值：希望系统达到的值；本项目中表示目标转速，单位 RPM。 */
	float Actual;       /* 当前实际值：传感器测得的反馈；本项目中表示编码器转速，单位 RPM。 */
	float Actual1;      /* 上一次实际值：用于计算实际值的变化速度，也就是微分项。 */
	float Out;          /* PID 最终输出；本项目中作为带方向的 PWM 占空比命令。 */

	float Kp;           /* 比例系数：误差出现后，立即给出多大的纠正力度。 */
	float Ki;           /* 积分系数：持续累加误差，用来消除长期存在的小偏差。 */
	float Kd;           /* 微分系数：根据实际值的变化趋势提前“刹车”，减小超调。 */

	float Error0;       /* 当前误差，计算方法为 Target - Actual。 */
	float Error1;       /* 上一次误差；当前版本会保存它，便于后续调试或扩展算法。 */

	float POut;         /* 比例项输出：Kp × 当前误差。 */
	float IOut;         /* 积分项输出：历史误差的累计结果。 */
	float DOut;         /* 微分项输出：当前版本根据实际值的变化量计算。 */

	float IOutMax;      /* 积分项允许的最大值，用于减轻积分饱和。 */
	float IOutMin;      /* 积分项允许的最小值，通常等于 -IOutMax。 */

	float OutMax;       /* 最终输出最大值，防止命令超过执行器能力。 */
	float OutMin;       /* 最终输出最小值，通常等于 -OutMax。 */

	float OutOffset;    /* 输出死区补偿：输出非零时额外增加的基础驱动力。 */
} PID_t;

/*
 * 四轮速度环共用的参数配置。
 * 它只描述“准备使用什么参数”，不保存任何电机的运行历史。
 * 修改此配置后，需要调用 Motor_RefreshPidProfiles()，或逐个调用
 * PID_ApplySpeedConfig()，新参数才会进入具体的 PID_t 实例。
 */
typedef struct
{
	float kp;               /* 共用比例系数。 */
	float ki;               /* 共用积分系数。 */
	float kd;               /* 共用微分系数。 */
	float integralLimit;    /* 积分项的正负限幅绝对值。 */
	float outputLimit;      /* 最终输出的正负限幅绝对值。 */
	float outputOffset;     /* 输出死区补偿值；当前默认配置为 0。 */
} PID_SpeedConfig_t;

/* 从 BoardConfig.h 读取速度环默认参数，保存到模块内部的共用配置中。 */
void PID_LoadSpeedConfig(void);

/* 读取当前共用配置；config 为空指针时函数会直接返回。 */
void PID_GetSpeedConfig(PID_SpeedConfig_t *config);

/* 修改共用配置中的 Kp、Ki、Kd；此函数不会自动更新已有的 PID_t 实例。 */
void PID_SetSpeedTunings(float kp, float ki, float kd);

/* 修改积分限幅和总输出限幅；传入负数时会按其绝对值处理。 */
void PID_SetSpeedLimits(float integralLimit, float outputLimit);

/* 首次使用前初始化 PID 实例；会清空参数和全部运行状态。 */
void PID_Init(PID_t *p);

/* 清空目标、反馈、误差、积分和输出，但保留 PID 参数及限幅。 */
void PID_Reset(PID_t *p);

/* 把当前共用速度环配置复制到一个具体的 PID 实例。 */
void PID_ApplySpeedConfig(PID_t *p);

/* 使用 p 中已有的 Target 和 Actual 执行一次 PID 运算。 */
void PID_Update(PID_t *p);

/* 设置本次目标值和实际值，执行一次 PID 运算，并返回最终输出。 */
float PID_Calculate(PID_t *p, float target, float actual);

#endif
