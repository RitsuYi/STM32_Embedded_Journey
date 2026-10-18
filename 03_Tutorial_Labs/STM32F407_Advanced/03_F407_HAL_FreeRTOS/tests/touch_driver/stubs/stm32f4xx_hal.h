#ifndef TOUCH_TEST_HAL_H
#define TOUCH_TEST_HAL_H
#include <stdint.h>
#include <stddef.h>
/* 仅主机测试使用的最小HAL类型；固件仍包含ST官方头文件。 */
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef unsigned GPIO_TypeDef;
typedef struct { uint32_t SR1, SR2; } I2C_TypeDef;
typedef struct { I2C_TypeDef *Instance; } I2C_HandleTypeDef;
typedef struct { uint32_t Pin, Mode, Pull, Speed, Alternate; } GPIO_InitTypeDef;
extern GPIO_TypeDef test_bus_port;
#define GPIOB (&test_bus_port)
#define GPIO_PIN_10 (1U << 10)
#define GPIO_PIN_11 (1U << 11)
#define GPIO_MODE_OUTPUT_OD 1U
#define GPIO_MODE_AF_OD 2U
#define GPIO_NOPULL 0U
#define GPIO_SPEED_FREQ_VERY_HIGH 3U
#define GPIO_AF4_I2C2 4U
#define HAL_I2C_ERROR_NONE 0U
#define HAL_I2C_ERROR_BERR 1U
#define HAL_I2C_ERROR_ARLO 2U
#define HAL_I2C_ERROR_AF 4U
#define HAL_I2C_ERROR_OVR 8U
#define HAL_I2C_ERROR_TIMEOUT 0x20U
#define I2C_FLAG_BUSY 2U
#define RESET 0U
#define __HAL_I2C_GET_FLAG(bus, flag) (((bus)->Instance->SR2 & (flag)) ? 1U : 0U)
void test_bus_reset(int asserted);
#define __HAL_RCC_I2C2_FORCE_RESET() test_bus_reset(1)
#define __HAL_RCC_I2C2_RELEASE_RESET() test_bus_reset(0)
#define I2C_MEMADD_SIZE_8BIT 1U
uint32_t HAL_GetTick(void);
uint32_t __get_IPSR(void);
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *bus);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);
void HAL_GPIO_Init(GPIO_TypeDef *port, const GPIO_InitTypeDef *pins);
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *bus);
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *bus);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *bus, uint16_t address,
    uint16_t reg, uint16_t address_size, uint8_t *data, uint16_t size, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *bus, uint16_t address,
    uint16_t reg, uint16_t address_size, uint8_t *data, uint16_t size, uint32_t timeout);
#endif
