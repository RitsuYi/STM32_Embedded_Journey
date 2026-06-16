#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f10x.h"

void Buzzer_Init(void);
void Buzzer_Stop(void);
void Buzzer_PlaySuccess(void);
void Buzzer_PlayAlarm(void);
void Buzzer_Update(uint32_t nowMs);

#endif

