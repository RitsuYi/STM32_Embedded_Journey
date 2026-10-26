#include "stm32f4xx_hal.h"
extern GPIO_TypeDef test_port;
#define LCD_RST_GPIO_Port (&test_port)
#define LCD_BL_GPIO_Port (&test_port)
#define FLASH_CS_GPIO_Port (&test_port)
#define LCD_RST_Pin 3U
#define LCD_BL_Pin 4U
#define FLASH_CS_Pin 5U
