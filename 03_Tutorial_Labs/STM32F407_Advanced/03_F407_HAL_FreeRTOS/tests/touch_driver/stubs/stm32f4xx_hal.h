#ifndef TOUCH_TEST_HAL_H
#define TOUCH_TEST_HAL_H
#include <stdint.h>
#include <stddef.h>
/* 仅主机测试使用的最小HAL类型；固件仍包含ST官方头文件。 */
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef unsigned GPIO_TypeDef;
typedef unsigned I2C_HandleTypeDef;
#define HAL_I2C_ERROR_NONE 0U
#define I2C_MEMADD_SIZE_8BIT 1U
uint32_t HAL_GetTick(void);
uint32_t __get_IPSR(void);
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *bus);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *bus, uint16_t address,
    uint16_t reg, uint16_t address_size, uint8_t *data, uint16_t size, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *bus, uint16_t address,
    uint16_t reg, uint16_t address_size, uint8_t *data, uint16_t size, uint32_t timeout);
#endif
