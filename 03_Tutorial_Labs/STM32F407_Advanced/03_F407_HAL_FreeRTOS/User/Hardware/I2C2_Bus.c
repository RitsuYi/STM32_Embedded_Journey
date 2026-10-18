#include "I2C2_Bus.h"
#include "main.h"
#include "i2c.h"
#include "task.h"
#include "semphr.h"

/* 静态创建表示锁本身占固定内存，不额外申请FreeRTOS堆。
 * 必须在main完成OLED等HAL启动操作后、启动任务前创建。
 * 不提前创建，避免本工程FreeRTOS端口在调度前屏蔽TIM6，卡住HAL延时。 */
static StaticSemaphore_t mutex_storage;
static SemaphoreHandle_t mutex;
static bool have_recovery_time;
static uint32_t last_recovery_ms;

/* 这里只使用已核实的I2C2引脚，不碰触摸复位和LCD/SPI。
 * 开漏输出的“高”表示放开线路，仍由现有3.3V上拉电阻提供高电平。 */
#define BUS_SCL GPIO_PIN_10
#define BUS_SDA GPIO_PIN_11

static uint32_t line_levels(void)
{
    return (HAL_GPIO_ReadPin(GPIOB, BUS_SCL) == GPIO_PIN_SET ? 2U : 0U) |
           (HAL_GPIO_ReadPin(GPIOB, BUS_SDA) == GPIO_PIN_SET ? 1U : 0U);
}

/* 每次只等待有限的节拍：从机若一直拉低SCL，退出而不是卡在while里。
 * 1ms再加一节拍，保证实际高/低保持时间远大于100kHz总线的微秒级最小值。
 * 任务延时允许按键、LED等继续运行，但总线互斥锁一直保留。 */
static bool release_clock(void)
{
    HAL_GPIO_WritePin(GPIOB, BUS_SCL, GPIO_PIN_SET);
    for (unsigned i = 0; i < 3U; ++i) {
        vTaskDelay(pdMS_TO_TICKS(1) + 1U);
        if (HAL_GPIO_ReadPin(GPIOB, BUS_SCL) == GPIO_PIN_SET) return true;
    }
    return false;
}

static bool recover_bus(void)
{
    /* HAL超时会把软件State设回READY，却不一定发出STOP、释放硬件BUSY。
     * 先关闭控制器，再让卡在半个字节的从机走完时钟。NXP UM10204 3.1.16
     * 规定SDA被拉低时最多补9个时钟；SCL仍低则失败退出，不能强推高电平。 */
    bool deinit_ok = HAL_I2C_DeInit(&hi2c2) == HAL_OK;
    __HAL_RCC_I2C2_FORCE_RESET();
    __HAL_RCC_I2C2_RELEASE_RESET();
    HAL_GPIO_WritePin(GPIOB, BUS_SCL | BUS_SDA, GPIO_PIN_SET);
    GPIO_InitTypeDef pins = {0};
    pins.Pin = BUS_SCL | BUS_SDA;
    pins.Mode = GPIO_MODE_OUTPUT_OD;
    pins.Pull = GPIO_NOPULL;
    pins.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &pins);
    bool clock_free = release_clock();
    for (unsigned i = 0; clock_free && i < 9U &&
         HAL_GPIO_ReadPin(GPIOB, BUS_SDA) == GPIO_PIN_RESET; ++i) {
        HAL_GPIO_WritePin(GPIOB, BUS_SCL, GPIO_PIN_RESET);
        vTaskDelay(pdMS_TO_TICKS(1) + 1U);
        clock_free = release_clock();
    }
    if (clock_free) {
        /* STOP：SCL高电平时把SDA从低释放到高，让两台从机结束旧事务。
         * 先拉低SCL再拉低SDA，避免在这里额外产生一个START。 */
        HAL_GPIO_WritePin(GPIOB, BUS_SCL, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, BUS_SDA, GPIO_PIN_RESET);
        vTaskDelay(pdMS_TO_TICKS(1) + 1U);
        clock_free = release_clock();
    }
    HAL_GPIO_WritePin(GPIOB, BUS_SDA | BUS_SCL, GPIO_PIN_SET);
    vTaskDelay(pdMS_TO_TICKS(1) + 1U);
    uint32_t recovered_lines = line_levels();
    /* 无论恢复成功还是失败，都归还AF4功能。沿用100kHz等原Init参数；
     * 不调用会在失败时死循环的MX_I2C2_Init，也不修改CubeMX生成源码。 */
    pins.Mode = GPIO_MODE_AF_OD;
    pins.Alternate = GPIO_AF4_I2C2;
    HAL_GPIO_Init(GPIOB, &pins);
    bool init_ok = HAL_I2C_Init(&hi2c2) == HAL_OK;
    return deinit_ok && init_ok && clock_free && recovered_lines == 3U &&
           __HAL_I2C_GET_FLAG(&hi2c2, I2C_FLAG_BUSY) == RESET;
}

bool I2C2_Bus_Init(void)
{
    if (!mutex) mutex = xSemaphoreCreateMutexStatic(&mutex_storage);
    return mutex != NULL;
}

bool I2C2_Bus_Lock(TickType_t timeout_ticks)
{
    if (__get_IPSR() != 0U) return false;

    BaseType_t state = xTaskGetSchedulerState();
    /* 此时仅有main，允许锁尚未创建；不调用任何创建/取得信号量的接口，
     * 保持TIM6中断可用，让OLED_Init里的HAL_Delay和HAL通信超时正常计时。 */
    if (state == taskSCHEDULER_NOT_STARTED) return true;
    /* 任务期必须有锁；调度器暂停时也不能等待其他任务。 */
    if (state != taskSCHEDULER_RUNNING || !mutex) return false;
    return xSemaphoreTake(mutex, timeout_ticks) == pdTRUE;
}

void I2C2_Bus_ReportResult(uint16_t address, HAL_StatusTypeDef status)
{
    /* 地址参数保留以兼容现有OLED/触摸调用；恢复判断只依赖本次HAL状态及错误位。 */
    (void)address;
    if (status == HAL_OK) return;
    uint32_t error = HAL_I2C_GetError(&hi2c2);
    /* 设备不应答(AF/NACK)本身不要求重置整条共享总线。只有HAL已结束的
     * 超时/忙或控制器故障才恢复，不能因为GUI取锁超时就破坏OLED正在发送的页。 */
    bool controller_fault = status == HAL_BUSY || status == HAL_TIMEOUT ||
        (error & (HAL_I2C_ERROR_TIMEOUT | HAL_I2C_ERROR_BERR |
                  HAL_I2C_ERROR_ARLO | HAL_I2C_ERROR_OVR)) != 0U;
    if (!controller_fault || __get_IPSR() != 0U ||
        xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) return;
    uint32_t now = HAL_GetTick();
    if (have_recovery_time && now - last_recovery_ms < 1000U) return;
    have_recovery_time = true;
    last_recovery_ms = now;
    /* 恢复只处理总线状态，不重发失败数据；本次调用的原错误仍交给调用者处理。 */
    (void)recover_bus();
}

void I2C2_Bus_Unlock(void)
{
    if (__get_IPSR() == 0U && mutex &&
        xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        (void)xSemaphoreGive(mutex);
}
