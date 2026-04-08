#include "myfile.h"

/************************元素***************************/

//变量定义
uint8_t Element_Flag=0;
uint8_t Ten_Flag=0;
uint8_t Ring_Flag=0;
uint8_t Noline_Flag=0;
uint8_t Stop_Flag = 0;
uint8_t K=0;
int Speed_Choice[3]={110,0,130};



/************************元素控制台***************************/
//在这里启动或者关掉某个元素

void Element_Process()
{
    Element_Normal();		//正常状态下，默认打开转向环和速度环
		Element_Ring();			//圆环
		Element_Noline();		//丢线处理
		Element_Stop();			//停车
		Element_Ten();			//十字识别（但未处理）
}


void Element_Normal()
{
 if(Element_Flag==0)
 {
	 Place_Enable=1;
	 PWM_Enable=1;
	 Basic_Speed=Speed_Choice[0];
 }
}

//圆环
void Element_Ring()
{
	if(Element_Flag==0&&Ring_Flag==0&&Noline_Flag==4&&Ten_Flag==1)	//调试状态下可以将&&Noline_Flag==4&&Ten_Flag==1移出，这里我是比赛为了防止误触，做了一个元素顺序限制
	{
			//第一次检测入环
			if(R2==1&&M==1&&(L1==0||R1==0)&&L2==0)//初次检测
		{		
				Ring_Flag=1;	
				Clear_Location();			
		}
	}
		//第一次检测到后过10cm再进行检测然后入环
	if(Ring_Flag==1&&Location>=10.5)
	{		
		if((L1==0&&M==1&&R1==0)||((L1==1||R1==1)&&M==1))
		{
			Ring_Flag=2;
			Clear_Location();	
			Element_Flag=1;	
			Place_Enable=0;//关掉转向环
			Place_Out=-15; //给一个固定的差速
		}
		else{Ring_Flag=0;}
	}
		//结束入环动作，正常巡线
	if(Ring_Flag==2&&Location>30)
	{
		Place_Enable=1;
		Ring_Flag=3;
		Basic_Speed=Speed_Choice[2];
		
	}
	//出环动作和入环同理
	if(Ring_Flag==3&&Location>380)
	{
		if(L2==1&&M==1&&R2==0)
		{
			Clear_Location();	
			Place_Enable=0;
			Place_Out=-15;	
			Ring_Flag=4;			
		}
	}
	//出环结束
	if(Ring_Flag==4&&Location>50)
	{
		Element_Flag=0;
		K=1;
		Ring_Flag=5;
		Clear_Location();
		
	}
}


//丢线   
void Element_Noline()
{
		if (Element_Flag == 0 &&Noline_Flag==0)
		{
			if (L2 == 0 && L1 == 0 && M == 0 && R1 == 0 && R2 == 0)  //丢线的原理就是所有灯熄灭
	      { 	  
					Noline_Flag = 1;
					Clear_Location();					//当初次识别丢线之后开始计算路程
	      }  								
    }
	if(Noline_Flag==1)	
	{
			if(L2==0&&L1==0&&M==0&&R1==0&&R2==0)
			{
				if(Location>19)							//当累计路程大于19，正式进入丢线，否则算作误判，不进入丢线
				{
				Element_Flag=2;
				Place_Enable=0;							
				Place_Out= -100;						//首先大幅度转向到你想要的方向
				Noline_Flag=2;	
				Clear_Location();						//清零编码器，通过距离来判断转了多少，（用陀螺仪也可以）
				}		
			}
			else
				{
					Clear_Location();	
					Noline_Flag=0;
				}	
	}
	if(Noline_Flag==2&&Location>25)		//转到想要的方向后，走直线加速冲过去
	{
		Noline_Flag=3;
		PWM_Enable=0;
		Motor_SetPWM_L(6000);						//设置固定的占空比
		Motor_SetPWM_R(6000);  	
	}	
	if(Noline_Flag==3&&Location>=50&&(L2==1||L1==1||M==1||R1==1||R2==1))//识别到线之后，恢复正常寻线
		{
			
			Noline_Flag=4;
			Element_Flag=0;
			Clear_Location();	
		}
}
//停车  
void Element_Stop()
{
		if (Element_Flag == 0&&Stop_Flag==0&&Ring_Flag==5) 					//我的停车是在最后一个元素圆环后，这样不会和断路误判
		{
			if (L2 == 1 && L1 == 1 && M == 1 && R1 == 1 && R2 == 1)		//仍然是全灭，这里可能和丢线重叠，需要做好标志位的限制
	      { 
					Stop_Flag = 1;  
			  	Clear_Location();						
         }
		}
		if (Stop_Flag == 1&&Location >=30) 
		{
			Place_Enable=0;
			Basic_Speed=Speed_Choice[1];
		}
		
}
//十字识别，辅助判断圆环，我的圆环在十字之后，根据赛道情况自己调整
void Element_Ten()
{
 if(Element_Flag==0&&Ring_Flag==0&&Noline_Flag==4)
 {
	if (L2 == 1 && L1 == 1 && M == 1 && R1 == 1 && R2 == 1)
	{
		Ten_Flag=1;
	}

 }
}
