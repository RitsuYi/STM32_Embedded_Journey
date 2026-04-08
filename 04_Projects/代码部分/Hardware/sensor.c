#include "sensor.h"

/*************************************
*鍑芥暟鍚嶇О锛歋ENSOR_GPIO_Config
*鍑芥暟鍔熻兘锛欸PIO绠¤剼鐨勯厤缃?
*鍙傛暟锛?
*璇存槑锛?
*			
**************************************/
void SENSOR_GPIO_Config(void)
{		
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);	// 释放 PB4，保留 SWD

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}
/*************************************
*鍑芥暟鍚嶇О锛歞igtal
*鍑芥暟鍔熻兘锛氳幏鍙朮閫氶亾鏁板瓧鍊?
*鍙傛暟锛?
*璇存槑锛?
*			
**************************************/
unsigned char digital(unsigned char channel)//1-ADC_N	  鑾峰彇X閫氶亾鏁板瓧鍊?
{
	u8 value = 0;
	switch(channel) 
	{
		case 1:  
			if(PAin(2) == 1) value = 1;
			else value = 0;  
			break;  
		case 2: 
			if(PAin(3) == 1) value = 1;
			else value = 0;  
			break;  
		case 3: 
			if(PAin(4) == 1) value = 1;
			else value = 0;  
			break;   
		case 4:  
			if(PBin(4) == 1) value = 1;
			else value = 0;  
			break;   
		case 5:
			if(PBin(5) == 1) value = 1;
			else value = 0;  
			break;
	}
	return value; 
}




