//#include "stm32f10x.h"  // Device header
//#include "Delay.h"
//#include "OLED.h"
//#include "LED.h"
//#include "Timer.h"
//#include "Motor.h"
//#include "Encoder.h"
//#include "Serial.h"
//#include "BlueSerial.h"
//#include "PID.h"
//#include <string.h>
//#include <stdlib.h>
//#include<math.h>

//#define SPEED_LOOP_DT 0.01f   // 10ms

//// 定义定时器错误标志（修复之前的报错）
//uint8_t TimerErrorFlag = 0;
//uint16_t TimerCount = 0;

//uint8_t RunFlag;  // 运行标志保留

//int16_t LeftPWM,RightPWM;
//int16_t	AvePWM,DifPWM;

//float LeftSpeed,RightSpeed;
//float AveSpeed,DifSpeed;

//PID_t SpeedPID={
//	.Kp=2.53,
//	.Ki=0.10,
//	.Kd=0,
//	.OutMax=10,
//	.OutMin=-10,
//};

//int main()
//{
//	OLED_Init();
//	// MPU6050_Init();  不需要也可以注释
//	LED_Init();
//	// Key_Init();     已删除
//	Motor_Init();
//	Encoder_Init();
//	Serial_Init();
//	Timer_Init();
//	BlueSerial_Init();
//	
//	RunFlag = 1; // 默认启动电机（不想启动就写 0）
//	
//	while(1)
//	{
//		OLED_Printf(50,0,OLED_6X8,"  Speed");	
//		OLED_Printf(50,8,OLED_6X8,"%05.2f",SpeedPID.Kp);
//		OLED_Printf(50,16,OLED_6X8,"%05.2f",SpeedPID.Ki);
//		OLED_Printf(50,24,OLED_6X8,"%05.2f",SpeedPID.Kd);
//		OLED_Printf(50,32,OLED_6X8,"%+05.1f",SpeedPID.Target);
//		OLED_Printf(50,40,OLED_6X8,"%+05.1f",AveSpeed);
//		OLED_Printf(50,48,OLED_6X8,"%+05.0f",SpeedPID.Out);
//		OLED_Update();
//		
//		if (BlueSerial_RxFlag == 1)
//		{
//			char *Tag = strtok(BlueSerial_RxPacket, ",");
//			if (strcmp(Tag, "key") == 0)
//			{
//				// 已删除按键逻辑
//			}
//			else if (strcmp(Tag, "slider") == 0)
//			{
//				char *Name = strtok(NULL, ",");
//				char *Value = strtok(NULL, ",");
//				
//				if(strcmp(Name,"SpeedKp")==0)
//				{
//					SpeedPID.Kp=atof(Value);
//				}
//				else if(strcmp(Name,"SpeedKi")==0)
//				{
//					SpeedPID.Ki=atof(Value);
//				}
//				else if(strcmp(Name,"SpeedKd")==0)
//				{
//					SpeedPID.Kd=atof(Value);
//				}
//			}
//			else if (strcmp(Tag, "joystick") == 0)
//			{
//				int8_t LH = atoi(strtok(NULL, ","));
//				int8_t LV = atoi(strtok(NULL, ","));
//				int8_t RH = atoi(strtok(NULL, ","));
//				int8_t RV = atoi(strtok(NULL, ","));
//				
//				SpeedPID.Target=LV/25.0;
//				DifPWM=0;
//			}
//			
//			BlueSerial_RxFlag = 0;
//		}

//		BlueSerial_Printf("[plot,%f,%f]",SpeedPID.Target,AveSpeed);
//	}
//}

//void TIM1_UP_IRQHandler(void)
//{
//    static uint16_t SpeedLoopCnt = 0;
//    
//    if(TIM_GetITStatus(TIM1, TIM_IT_Update) == SET)
//    {
//        TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
//        
//        TimerCount++;  // 定时器计数
//        
//        /* ================= 电机输出 ================= */
//        if(RunFlag)
//        {
//            LeftPWM  = AvePWM + DifPWM;
//            RightPWM = AvePWM - DifPWM;
//            
//            if(LeftPWM > 100) LeftPWM = 100;
//            if(LeftPWM < -100) LeftPWM = -100;
//            if(RightPWM > 100) RightPWM = 100;
//            if(RightPWM < -100) RightPWM = -100;
//            
//            Motor_SetPWM(1, LeftPWM);
//            Motor_SetPWM(2, RightPWM);
//        }
//        else
//        {
//            Motor_SetPWM(1, 0);
//            Motor_SetPWM(2, 0);
//        }

//        /* ================= 速度环（10ms） ================= */
//        SpeedLoopCnt++;
//        if(SpeedLoopCnt >= 10)
//        {
//            SpeedLoopCnt = 0;
//            
//            LeftSpeed  = Encoder_Get(1) * (1.0f / 44.0f) /9.27666;
//            RightSpeed = Encoder_Get(2) * (1.0f / 44.0f) /9.27666;
//            
//            AveSpeed = (LeftSpeed + RightSpeed) / 2.0f;
//            DifSpeed = LeftSpeed - RightSpeed;
//            
//            if(RunFlag)
//            {
//                SpeedPID.Actual = AveSpeed;
//                PID_Update(&SpeedPID);
//                AvePWM = SpeedPID.Out;
//            }
//        }
//        
//        /* ================= 中断保护 ================= */
//        if(TIM_GetITStatus(TIM1, TIM_IT_Update) == SET)
//        {
//            TimerErrorFlag = 1;
//            TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
//        }
//    }
//}


#include "stm32f10x.h"  // Device header
#include "Delay.h"
#include "OLED.h"
#include "Motor.h"
#include "PWM.h"
#include "Car.h"
#include "Track.h"
#include "Encoder.h"

int main(void)
{
	
	Encoder_Init();
    Car_Init();
	Motor_Init();
	OLED_Init();
	PWM_Init();
	Track_Init();
   

    while (1)
    {
		Track_Run();
    }
}
