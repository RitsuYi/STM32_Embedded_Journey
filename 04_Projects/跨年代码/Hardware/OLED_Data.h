#ifndef __OLED_DATA_H
#define __OLED_DATA_H

#include <stdint.h>

/*中文字符字节宽度*/
#define OLED_CHN_CHAR_WIDTH			3		//UTF-8编码格式给3，GB2312编码格式给2

/*字模基本单元*/
typedef struct 
{
	char Index[OLED_CHN_CHAR_WIDTH + 1];	//汉字索引
	uint8_t Data[32];						//字模数据
} ChineseCell_t;

/*ASCII字模数据声明*/
extern const uint8_t OLED_F8x16[][16];
extern const uint8_t OLED_F6x8[][6];
extern const uint8_t OLED_F12x24[][36];
/*汉字字模数据声明*/
extern const ChineseCell_t OLED_CF16x16[];

/*图像数据声明*/
extern const uint8_t Diode[];
/*按照上面的格式，在这个位置加入新的图像数据声明*/
//...
extern const uint8_t shuzi_1[];
extern const uint8_t shuzi_2[];
extern const uint8_t shuzi_3[];
extern const uint8_t shuzi_4[];
extern const uint8_t shuzi_5[];
extern const uint8_t shuzi_5_1[];
extern const uint8_t shuzi_4_1[];
extern const uint8_t shuzi_3_1[];
extern const uint8_t shuzi_2_1[];
extern const uint8_t shuzi_1_1[];
extern const uint8_t hanzi_xin[];
extern const uint8_t hanzi_de[];
extern const uint8_t hanzi_yi[];
extern const uint8_t hanzi_nian[];
extern const uint8_t hanzi_zhu[];
extern const uint8_t hanzi_ni[];

extern const uint8_t hanzi_hao[];
extern const uint8_t hanzi_yun[];
extern const uint8_t hanzi_xing[];
extern const uint8_t hanzi_fu[];
extern const uint8_t hanzi_jian[];
extern const uint8_t hanzi_kang[];
extern const uint8_t hanzi_cai[];
extern const uint8_t hanzi_fu1[];
extern const uint8_t fangkuang[];
#endif


/*****************江协科技|版权所有****************/
/*****************jiangxiekeji.com*****************/
