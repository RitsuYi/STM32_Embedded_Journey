#include "myfile.h"

/************************菜单***************************/
uint8_t Key_Num=0;
uint8_t func_index = 0;
uint8_t Cursor=1;
int Key_Flag=0;

/************************内部配置***************************/
typedef struct
{ 
    uint8_t Current;	//当前状态索引号
    uint8_t Up;      		  //
		uint8_t Down;     	//	
    void (*current_operation)(void); //当前状态应该执行的操作
} Key_table;

void (*current_operation_index)(); //定义一个函数指针

Key_table table[100]=
{	
  {0,4,1,(*Boot_animation)},  
  {1,0,2,(*Homepage_1)},
  {2,1,3,(*Homepage_2)},
  {3,2,4,(*Homepage_3)},
  {4,3,0,(*Homepage_4)},

};
void menu_operation()
{	

	if(Key_Num==1)//左
	{         
		func_index = table[func_index].Up;
		OLED_Clear();        		
	}
	if(Key_Num==2)  //右
	{   
		func_index = table[func_index].Down;
		OLED_Clear();        		
	}
    current_operation_index=table[func_index].current_operation;//执行当前索引号所对应的功能函数。
    (*current_operation_index)();//执行当前操作函数		
}


/************************显示部分***************************/
//开机动画可以自己设置
void Boot_animation()
{
	OLED_ShowString(40,32,"Hello!!!",OLED_8X16);
}

//第一页
void Homepage_1()	
{
		OLED_Printf(0,1,OLED_8X16,"	Speed_L:%+05d",Speed_L);
		OLED_Printf(0,17,OLED_8X16,"Speed_R:%+05d",Speed_R);
		OLED_Printf(0,34,OLED_8X16,"PWM_L:%+02d",Speed_Out_L);
		OLED_Printf(0,51,OLED_8X16,"PWM_R:%+02d",Speed_Out_R);
		OLED_Update();
}
//第一页
void Homepage_2()	
{
		OLED_Printf(0,0,OLED_8X16,"	s_err:%+04d",sensor_err);
		OLED_Printf(0,17,OLED_8X16,"f_err:%+04d",final_err);
		OLED_Printf(0,34,OLED_8X16,"P_Out:%+04d",Place_Out);
		OLED_Printf(0,51,OLED_8X16,"gz:%+03d",GZ);
		OLED_Update();
}
//第三页
void Homepage_3()	
{
			OLED_Printf(0,0,OLED_8X16,"	s:%+06.2f",Location);
  		    OLED_Printf(0,17,OLED_8X16,"yaw:%+03.2f",yaw);
			OLED_Printf(0,34,OLED_8X16,"FE:%+02d",Element_Flag);
			OLED_Printf(0,51,OLED_8X16,"FR:%+02d",Ring_Flag);
}
//第四页
void Homepage_4()	
{
			OLED_Printf(0,0,OLED_8X16,"	FP:%+03d",Place_Enable);
			OLED_Printf(0,17,OLED_8X16,"FN:%+02d",Noline_Flag);
			OLED_Printf(0,34,OLED_8X16,"FT:%+02d",Ten_Flag);
			OLED_Printf(0,51,OLED_8X16,"FS:%+02d",Stop_Flag);

	
}


