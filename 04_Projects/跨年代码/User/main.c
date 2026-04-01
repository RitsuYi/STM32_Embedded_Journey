/***************************************************************************************
  * 本页程序由本人创建开源共享
  * 程序名称：				0.96寸oled显示2026跨年祝福语
  * 程序创建时间：			2025.12.3
	* 本项目使用模块化编程，重要模块如下：
	* 主控：stm32
	* oled模块：底层代码由江协科技编写
	* MyRTC：实时时钟外设代码
	* 说明：本程序在oled上显示的字母、汉字、数字、图像需在取模软件上进行取模，因大小不一致
	* 故部分字母、汉字或数字是以图像的方式显示
	* 若想自定义汉字、图像等及其大小，推荐以图像的方式进行取模
	* 具体操作步骤可观看b站江协科技模块化教程(oled快速上手视频)
  ***************************************************************************************
  */
#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "MyRTC.h"
int16_t x1,x2,x3;
int main ()
{
		OLED_Init();
		MyRTC_Init ();
		MyRTC_SetTime ();
/***************************正常时间显示***********************************/
		while (1)
		{
			MyRTC_ReadTime ();
			if (MyRTC_Time [1]==12 && MyRTC_Time [2]==31 && MyRTC_Time [3]==23 && MyRTC_Time [4]==59)//2025.12.31.23.59后时间显示
			{
				if (MyRTC_Time [5]<=54)
				{
					OLED_Printf (0,0,OLED_6X8,"%d-%02d-%02d",MyRTC_Time [0],MyRTC_Time [1],MyRTC_Time [2]);
					OLED_Printf (16,16,OLED_12X24,"%02d:%02d:%02d",MyRTC_Time [3],MyRTC_Time [4],MyRTC_Time [5]);
					OLED_Update ();
				}
				else
				{
					OLED_Clear();
					OLED_Update();
					break;
				}
			}
			else //正常时间显示
			{
				OLED_Printf (0,0,OLED_6X8,"%d-%02d-%02d",MyRTC_Time [0],MyRTC_Time [1],MyRTC_Time [2]);
				OLED_Printf (16,16,OLED_12X24,"%02d:%02d:%02d",MyRTC_Time [3],MyRTC_Time [4],MyRTC_Time [5]);
				OLED_Update ();
			}
		}
/***************************进入倒计时循环(5~0秒)***********************************/		
		while (1)
		{
			MyRTC_ReadTime ();
			OLED_Printf (0,0,OLED_6X8,"%d-%02d-%02d",MyRTC_Time [0],MyRTC_Time [1],MyRTC_Time [2]);
			OLED_Printf (0,56,OLED_6X8,"%02d:%02d:%02d",MyRTC_Time [3],MyRTC_Time [4],MyRTC_Time [5]);
			OLED_Update ();
			if (MyRTC_Time [5]==55)
			{
				OLED_ShowImage(57,25,12,25,shuzi_5_1);//显示小数字5
				OLED_Update();
				Delay_ms(100);
				OLED_ShowImage(50,7,25,50,shuzi_5);//显示数字5
				OLED_Update();
				Delay_ms(900);
				OLED_ClearArea (50,7,25,50);
			}
			if (MyRTC_Time [5]==56)
			{
				OLED_ShowImage(57,25,12,25,shuzi_4_1);//显示小数字4
				OLED_Update();
				Delay_ms(100);
				OLED_ShowImage(50,7,25,50,shuzi_4);//显示数字4
				OLED_Update();
				Delay_ms(900);
				OLED_ClearArea (50,7,25,50);
					
			}
			if (MyRTC_Time [5]==57)
			{
				OLED_ShowImage(57,25,12,25,shuzi_3_1);//显示小数字3
				OLED_Update();
				Delay_ms(100);
				OLED_ShowImage(50,7,25,50,shuzi_3);//显示数字3
				OLED_Update();
				Delay_ms(900);
				OLED_ClearArea (50,7,25,50);
					
			}
			if (MyRTC_Time [5]==58)
			{
				OLED_ShowImage(57,25,12,25,shuzi_2_1);//显示小数字2
				OLED_Update();
				Delay_ms(100);
				OLED_ShowImage(50,7,25,50,shuzi_2);//显示数字2
				OLED_Update();
				Delay_ms(900);
				OLED_ClearArea (50,7,25,50);
					
			}
			if (MyRTC_Time [5]==59)
			{
				OLED_ShowImage(57,25,12,25,shuzi_1_1);//显示小数字1
				OLED_Update();
				Delay_ms(100);
				OLED_ShowImage(50,7,25,50,shuzi_1);//显示数字1
				OLED_Update();
				Delay_ms(900);
				OLED_ClearArea (50,7,25,50);
					
			}
			if (MyRTC_Time [5]==0)
			{
				OLED_Clear ();
				OLED_Update();
				OLED_Printf (0,0,OLED_6X8,"%d-%02d-%02d",MyRTC_Time [0],MyRTC_Time [1],MyRTC_Time [2]);
				OLED_Printf (16,16,OLED_12X24,"%02d:%02d:%02d",MyRTC_Time [3],MyRTC_Time [4],MyRTC_Time [5]);
				OLED_Update ();
				Delay_ms(1000);
				OLED_Clear ();
				OLED_Update();
				break;
			}
		}	
/***************************倒计时循环结束，显示祝福动画***********************************/		
			OLED_ShowString(47,23,"2026",OLED_8X16);
			OLED_Update();
			Delay_ms(100);
			

			OLED_ShowString(35,23,"2 0 2 6",OLED_8X16);
			OLED_Update();
			Delay_ms(1000);
			OLED_Clear();
			OLED_Update();
	
			OLED_ShowChinese(32,23,"新年快乐");
			OLED_Update();
			Delay_ms(1000);
			OLED_Clear();
			OLED_Update();
				
			OLED_ShowImage(16,20,24,24,hanzi_xin);//显示汉字:新
			OLED_ShowImage(40,20,24,24,hanzi_de);	//显示汉字:的
			OLED_ShowImage(64,20,24,24,hanzi_yi);	//显示汉字:一
			OLED_ShowImage(88,20,24,24,hanzi_nian);	//显示汉字:年
			OLED_Update();
			Delay_ms(1000);
			OLED_Clear();
			OLED_Update();

			OLED_ShowImage(40,16,24,24,hanzi_zhu);//显示汉字:祝
			OLED_ShowString (64,16,"us",OLED_12X24);//显示英文:us
			OLED_Update();
			Delay_ms(1000);
			OLED_ShowImage(3,2,12,12,hanzi_hao);//显示汉字:好
			OLED_ShowImage(15,2,12,12,hanzi_yun);//显示汉字:运
			OLED_ShowString(27,5,"+1",OLED_6X8);
			OLED_Update();
			
			Delay_ms(1000);
			OLED_ShowImage(74,0,12,12,hanzi_xing);//显示汉字:幸
			OLED_ShowImage(86,0,12,12,hanzi_fu);//显示汉字:福	
			OLED_ShowString(98,3,"+1",OLED_6X8);
			OLED_Update();
			
			Delay_ms(1000);
			OLED_ShowImage(5,45,12,12,hanzi_jian);//显示汉字:健	
			OLED_ShowImage(17,45,12,12,hanzi_kang);//显示汉字:康
			OLED_ShowString(29,48,"+1",OLED_6X8);
			OLED_Update();
			
			Delay_ms(1000);
			OLED_ShowImage(78,43,12,12,hanzi_cai);//显示汉字:财
			OLED_ShowImage(90,43,12,12,hanzi_fu1);//显示汉字:富	
			OLED_ShowString(102,46,"+2",OLED_6X8);
			OLED_ShowString(116,41,"20",OLED_6X8);
			OLED_Update();
			
			Delay_ms(1000);
			OLED_Clear();
			OLED_Update();
			for (x1=32;x1>=-128;x1--)
			{
				x2=x1+80;
				x3=x2+80;
				OLED_ShowImage(27,19,74,26,fangkuang);
					
				OLED_ShowChinese(x1,23,"平安喜乐");
				OLED_ShowChinese(x2,23,"蒸蒸日上");
				OLED_ShowChinese(x3,23,"万事顺意");
				
				OLED_Update();
				if (x1==32 || x2==32 || x3==32)
				{
					Delay_ms(1000);
				}
				
			}
			
}
