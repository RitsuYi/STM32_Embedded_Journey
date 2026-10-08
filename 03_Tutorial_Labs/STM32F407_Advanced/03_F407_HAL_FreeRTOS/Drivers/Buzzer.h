#ifndef BUZZER_H
#define BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif

/* 调用前需完成 MX_GPIO_Init()；初始化后可调用 Buzzer_Off() 保持静音。 */

/* 开启蜂鸣器：PC8 输出低电平。 */
void Buzzer_On(void);

/* 关闭蜂鸣器：PC8 输出高电平。 */
void Buzzer_Off(void);

/* 翻转蜂鸣器当前的开关状态。 */
void Buzzer_Turn(void);

#ifdef __cplusplus
}
#endif

#endif /* BUZZER_H */
