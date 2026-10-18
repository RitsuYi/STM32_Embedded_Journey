#include <stdio.h>
#include <stdlib.h>
#include "I2C2_Bus.h"
#include "i2c.h"
#include "task.h"
#include "semphr.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

/* 用独立的“控制器、开漏输出、从机拉低”模型注入总线故障。
 * 测试真实恢复实现的有界退出/时序顺序/持锁约束，不证明电气波形已经实测。 */
GPIO_TypeDef test_bus_port;
static I2C_TypeDef controller;
I2C_HandleTypeDef hi2c2 = {.Instance = &controller};
static uint32_t now_ms, error, output = GPIO_PIN_10 | GPIO_PIN_11;
static unsigned pulses, stops, deinit_calls, init_calls, release_after;
static bool scl_stuck, sda_stuck, gpio_mode, fail_init;
static int scheduler = taskSCHEDULER_NOT_STARTED;
static SemaphoreHandle_t owner;

uint32_t HAL_GetTick(void) { return now_ms; }
uint32_t __get_IPSR(void) { return 0; }
BaseType_t xTaskGetSchedulerState(void) { return scheduler; }
void vTaskDelay(TickType_t ticks) { CHECK(owner->held); now_ms += ticks; }
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage)
{
    storage->held = 0; owner = storage; return storage;
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t ticks)
{
    (void)ticks;
    if (mutex->held) return 0;
    mutex->held = 1; return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex)
{
    CHECK(mutex->held && !gpio_mode); mutex->held = 0; return pdTRUE;
}
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *bus) { CHECK(bus == &hi2c2); return error; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    CHECK(port == GPIOB);
    if (pin == GPIO_PIN_10 && scl_stuck) return GPIO_PIN_RESET;
    if (pin == GPIO_PIN_11 && sda_stuck) return GPIO_PIN_RESET;
    return (output & pin) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
    CHECK(port == GPIOB && owner->held);
    uint32_t before = output;
    if (state == GPIO_PIN_SET) output |= pin;
    else output &= ~pin;
    if (gpio_mode && !(before & GPIO_PIN_10) && (output & GPIO_PIN_10) && !scl_stuck) {
        ++pulses;
        if (release_after && pulses >= release_after) sda_stuck = false;
    }
    if (gpio_mode && !(before & GPIO_PIN_11) && (output & GPIO_PIN_11) &&
        (output & GPIO_PIN_10) && !scl_stuck && !sda_stuck) ++stops;
}
void HAL_GPIO_Init(GPIO_TypeDef *port, const GPIO_InitTypeDef *pins)
{
    CHECK(port == GPIOB && owner->held && pins->Pin == (GPIO_PIN_10 | GPIO_PIN_11));
    gpio_mode = pins->Mode == GPIO_MODE_OUTPUT_OD;
    CHECK(gpio_mode || (pins->Mode == GPIO_MODE_AF_OD && pins->Alternate == GPIO_AF4_I2C2));
}
void test_bus_reset(int asserted)
{
    CHECK(owner->held);
    if (asserted) controller.SR1 = controller.SR2 = 0;
}
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *bus)
{
    CHECK(bus == &hi2c2 && owner->held); ++deinit_calls; return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *bus)
{
    CHECK(bus == &hi2c2 && owner->held && !gpio_mode); ++init_calls; error = 0;
    return fail_init ? HAL_ERROR : HAL_OK;
}
static void report(HAL_StatusTypeDef status, uint32_t flags)
{
    CHECK(I2C2_Bus_Lock(1));
    error = flags;
    I2C2_Bus_ReportResult(0x70, status);
    I2C2_Bus_Unlock();
}
int main(void)
{
    CHECK(I2C2_Bus_Init());
    /* 启动前不能进入RTOS延时；设备仅不应答也不能动整条总线。 */
    report(HAL_TIMEOUT, HAL_I2C_ERROR_TIMEOUT);
    CHECK(deinit_calls == 0);
    scheduler = taskSCHEDULER_RUNNING;
    report(HAL_OK, 0);
    report(HAL_ERROR, HAL_I2C_ERROR_AF);
    CHECK(deinit_calls == 0);

    /* 模拟原故障：软件返回TIMEOUT但硬件还BUSY，SDA卡在半个字节。
     * 第3个补时钟才放开SDA；必须产生STOP、恢复AF并重初始化。 */
    controller.SR1 = 0x1234; controller.SR2 = I2C_FLAG_BUSY;
    sda_stuck = true; release_after = 3;
    report(HAL_ERROR, HAL_I2C_ERROR_TIMEOUT);
    CHECK(deinit_calls == 1 && init_calls == 1 && !sda_stuck);
    CHECK(pulses == 4 && stops == 1 && deinit_calls == init_calls);
    CHECK(output == (GPIO_PIN_10 | GPIO_PIN_11) && !gpio_mode && !owner->held);

    /* 短时间反复失败最多恢复一次，正常事务和锁竞争不重置总线。 */
    report(HAL_BUSY, 0);
    CHECK(deinit_calls == 1);
    report(HAL_OK, 0);
    CHECK(deinit_calls == 1);
    CHECK(I2C2_Bus_Lock(1));
    CHECK(!I2C2_Bus_Lock(1));
    I2C2_Bus_Unlock();
    CHECK(deinit_calls == 1);

    /* 持续卡低不允许死循环，失败后也必须归还AF功能和总线锁。 */
    now_ms += 1000; pulses = stops = 0; sda_stuck = true; release_after = 0;
    uint32_t started = now_ms;
    report(HAL_TIMEOUT, HAL_I2C_ERROR_TIMEOUT);
    CHECK(pulses == 10 && stops == 0 && now_ms - started < 100);
    CHECK(deinit_calls == 2 && sda_stuck && !gpio_mode && !owner->held);
    now_ms += 1000; pulses = 0; scl_stuck = true;
    started = now_ms;
    report(HAL_BUSY, 0);
    CHECK(pulses == 0 && now_ms - started < 100 && !gpio_mode && !owner->held);
    CHECK(deinit_calls == 3 && scl_stuck);
    now_ms += 1000; scl_stuck = sda_stuck = false; fail_init = true;
    report(HAL_TIMEOUT, HAL_I2C_ERROR_TIMEOUT);
    CHECK(deinit_calls == 4 && init_calls == 4 && !gpio_mode && !owner->held);
    now_ms += 1000; fail_init = false;
    report(HAL_TIMEOUT, HAL_I2C_ERROR_TIMEOUT);
    CHECK(deinit_calls == 5 && deinit_calls == init_calls && !gpio_mode && !owner->held);
    puts("PASS: shared I2C timeout recovery, STOP, ownership, retry limit and stuck-line exit");
    return 0;
}
