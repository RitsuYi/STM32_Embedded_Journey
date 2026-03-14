#include "stm32f10x.h"                  // Device header
#include <time.h>
#include "MyRTC.h"
uint16_t MyRTC_Time []={2025,12,31,23,59,50};//时间数组，可自定义时间


void MyRTC_Init ()
{
	RCC_APB1PeriphClockCmd (RCC_APB1Periph_PWR,ENABLE);//开启PWR时钟
	RCC_APB1PeriphClockCmd (RCC_APB1Periph_BKP,ENABLE);//开启BKP备份寄存器时钟
	PWR_BackupAccessCmd(ENABLE);
	if (BKP_ReadBackupRegister(BKP_DR1) !=0xA5A5)
	{
		RCC_LSEConfig (RCC_LSE_ON);//使用LSE外部低速时钟
		while (RCC_GetFlagStatus (RCC_FLAG_LSERDY) !=SET);//获取LSE标志位状态
		RCC_RTCCLKConfig (RCC_RTCCLKSource_LSE);//选择时钟源
		RCC_RTCCLKCmd (ENABLE);
		
		RTC_WaitForSynchro ();//等待同步
		RTC_WaitForLastTask ();//等待上次写入操作完成
		
		RTC_SetPrescaler (32768-1);//预分频
		RTC_WaitForLastTask ();//等待上次写入操作完成

		MyRTC_SetTime ();
		
		BKP_WriteBackupRegister (BKP_DR1,0xA5A5);
	}
	else
	{
		RTC_WaitForSynchro ();//等待同步
		RTC_WaitForLastTask ();//等待上次写入操作完成
	}
	
}
void MyRTC_SetTime ()
{
	time_t time_cnt;
	struct tm time_date;//真实时间
	
	time_date.tm_hour =MyRTC_Time [3];
	time_date.tm_mday =MyRTC_Time [2];
	time_date.tm_min =MyRTC_Time [4];
	time_date.tm_mon =MyRTC_Time [1]-1;
	time_date.tm_sec =MyRTC_Time [5];
	time_date.tm_year =MyRTC_Time [0]-1900;
	
	time_cnt=mktime (&time_date)-8*60*60;//换算成时间戳秒数
	RTC_SetCounter (time_cnt);//时间戳秒数写入RTC
	RTC_WaitForLastTask ();//等待上次写入操作完成
}
void MyRTC_ReadTime ()
{
	time_t time_cnt;
	struct tm time_date;//真实时间
	
	time_cnt=RTC_GetCounter ()+8*60*60;//读取RTC时间戳秒数
	time_date=*localtime(&time_cnt);//时间戳秒数写入真实时间
	
	MyRTC_Time [3]=time_date.tm_hour;
	MyRTC_Time [2]=time_date.tm_mday;
	MyRTC_Time [4]=time_date.tm_min;
	MyRTC_Time [1]=time_date.tm_mon+1;
	MyRTC_Time [5]=time_date.tm_sec;
	MyRTC_Time [0]=time_date.tm_year+1900;
}