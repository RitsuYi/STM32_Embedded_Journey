#include "Key_Task.h"
#include "usart.h"

#include <string.h>


void KEY1_Task(void)
{
    HAL_UART_Transmit(&huart1,
                      (uint8_t *)"KEY1 Press\r\n",
                      strlen("KEY1 Press\r\n"),
                      50);
}


void KEY2_Task(void)
{
    HAL_UART_Transmit(&huart1,
                      (uint8_t *)"KEY2 Press\r\n",
                      strlen("KEY2 Press\r\n"),
                      50);
}


void KEY3_Task(void)
{
    HAL_UART_Transmit(&huart1,
                      (uint8_t *)"KEY3 Press\r\n",
                      strlen("KEY3 Press\r\n"),
                      50);
}