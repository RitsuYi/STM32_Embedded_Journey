#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "FT6336U.h"
#include "I2C2_Bus.h"
#include "main.h"
#include "i2c.h"
#include "task.h"
#include "semphr.h"

/* 用可控寄存器替代真实触摸芯片，重点验证错误不会变成按下、总线不会漏解锁。
 * 这些测试只验软件契约，不能证明I2C接线、实物方向或电容触摸性能。 */
GPIO_TypeDef test_port;
I2C_HandleTypeDef hi2c2;
static uint8_t registers[256];
static uint32_t now_ms, isr, hal_error, transfers, reset_low_ms, reset_high_ms;
static unsigned reset_edges, delays, takes, gives, creates;
static int scheduler = taskSCHEDULER_RUNNING;
static int deny_lock;
static HAL_StatusTypeDef injected_status = HAL_OK;
static GPIO_PinState interrupt_level = GPIO_PIN_SET;
static SemaphoreHandle_t bus_mutex;

uint32_t HAL_GetTick(void) { return now_ms; }
uint32_t __get_IPSR(void) { return isr; }
BaseType_t xTaskGetSchedulerState(void) { return scheduler; }
void vTaskDelay(TickType_t ticks) { now_ms += ticks; ++delays; }
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage)
{
    storage->held = 0;
    bus_mutex = storage;
    ++creates;
    return storage;
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t timeout)
{
    (void)timeout;
    ++takes;
    if (deny_lock || mutex->held) return 0;
    mutex->held = 1;
    return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex)
{
    assert(mutex->held);
    mutex->held = 0;
    ++gives;
    return pdTRUE;
}
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *bus)
{
    assert(bus == &hi2c2 && bus_mutex->held);
    return hal_error;
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    assert(port == TP_INT_GPIO_Port && pin == TP_INT_Pin);
    return interrupt_level;
}
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
    assert(port == TP_RST_GPIO_Port && pin == TP_RST_Pin);
    assert(!bus_mutex->held); /* 复位等待不能霸占OLED总线。 */
    if (state == GPIO_PIN_RESET) reset_low_ms = now_ms;
    else reset_high_ms = now_ms;
    ++reset_edges;
}
static void check_transfer(I2C_HandleTypeDef *bus, uint16_t address,
                           uint16_t address_size, uint32_t timeout)
{
    assert(bus == &hi2c2 && address == 0x70U);
    assert(address_size == I2C_MEMADD_SIZE_8BIT && timeout == 20U);
    assert(bus_mutex->held && scheduler == taskSCHEDULER_RUNNING && !isr);
    /* 芯片复位释放后必须等待到手册要求的报告时间。 */
    assert(now_ms - reset_high_ms >= 300U);
    ++transfers;
}
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *bus, uint16_t address,
    uint16_t reg, uint16_t address_size, uint8_t *data, uint16_t size, uint32_t timeout)
{
    check_transfer(bus, address, address_size, timeout);
    if (injected_status == HAL_OK) memcpy(data, registers + reg, size);
    return injected_status;
}
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *bus, uint16_t address,
    uint16_t reg, uint16_t address_size, uint8_t *data, uint16_t size, uint32_t timeout)
{
    check_transfer(bus, address, address_size, timeout);
    if (injected_status == HAL_OK) memcpy(registers + reg, data, size);
    return injected_status;
}

/* 生成真实报文格式，特意把手指编号写进YH高四位，检查驱动不会混进坐标。 */
static void frame(uint8_t count, uint8_t event, uint16_t x, uint16_t y)
{
    registers[2] = count;
    registers[3] = (uint8_t)((event << 6) | (x >> 8));
    registers[4] = (uint8_t)x;
    registers[5] = (uint8_t)(0xA0U | (y >> 8));
    registers[6] = (uint8_t)y;
}

int main(void)
{
    assert(!I2C2_Bus_Lock(1));
    /* 回归检查：OLED在任何RTOS对象创建前就要完成启动通信和HAL延时。
     * 此时只有main执行，允许不建锁访问，且不能偷偷创建/取得RTOS锁。
     * 调度器运行以后则相反：未创建锁必须拒绝访问，不能绕过互斥。 */
    scheduler = taskSCHEDULER_NOT_STARTED;
    assert(I2C2_Bus_Lock(1));
    I2C2_Bus_Unlock();
    assert(creates == 0 && takes == 0 && gives == 0);
    isr = 1;
    assert(!I2C2_Bus_Lock(1));
    isr = 0;
    scheduler = taskSCHEDULER_RUNNING;
    assert(!I2C2_Bus_Lock(1));
    scheduler = taskSCHEDULER_NOT_STARTED;
    /* 对应main已完成OLED启动、即将创建其他RTOS对象的位置。 */
    assert(I2C2_Bus_Init() && I2C2_Bus_Init() && creates == 1);
    assert(I2C2_Bus_Lock(1));
    I2C2_Bus_Unlock();
    assert(takes == 0 && gives == 0);
    scheduler = taskSCHEDULER_SUSPENDED;
    assert(!I2C2_Bus_Lock(1));
    scheduler = taskSCHEDULER_RUNNING;
    isr = 1;
    assert(!I2C2_Bus_Lock(1) && !FT6336U_Init());
    isr = 0;
    assert(I2C2_Bus_Lock(1));
    assert(!I2C2_Bus_Lock(1));
    I2C2_Bus_Unlock();

    registers[0xA3] = 0x64;
    registers[0xA6] = 0x12;
    registers[0xA8] = 0x11;
    registers[0] = registers[0xA4] = 0xFF;
    assert(FT6336U_Init());
    assert(reset_edges == 2 && delays == 2 && reset_high_ms - reset_low_ms >= 20U);
    assert(g_ft6336u.ready && g_ft6336u.chip_id == 0x64 && g_ft6336u.firmware_id == 0x12);
    assert(g_ft6336u.vendor_id == 0x11 && registers[0] == 0 && registers[0xA4] == 0);

    FT6336U_Point point;
    /* 用户实机确认屏幕Y需要反转；按独立端点表验证四角，防止修复时翻错轴。
     * 每行依次是原始X、原始Y、期望屏幕X、期望屏幕Y，不从配置宏计算期望值。 */
    const uint16_t corners[4][4] = {
        {0, 0, 0, 319}, {319, 0, 0, 0}, {0, 479, 479, 319}, {319, 479, 479, 0}
    };
    for (unsigned i = 0; i < 4; ++i) {
        frame(1, i ? 2 : 0, corners[i][0], corners[i][1]);
        assert(FT6336U_Read(&point) && point.pressed);
        assert(point.x == corners[i][2] && point.y == corners[i][3]);
    }
    assert(g_ft6336u.press_count == 1);
    /* 诊断显示必须反映实际时间差，不能固定填入配置的采样周期。 */
    now_ms += 75;
    interrupt_level = GPIO_PIN_RESET;
    frame(0, 0, 0, 0);
    assert(FT6336U_Read(&point) && !point.pressed && point.x == 479 && point.y == 0);
    assert(g_ft6336u.read_gap_ms == 75 && g_ft6336u.max_read_gap_ms == 75);
    frame(1, 1, 0, 0);
    assert(FT6336U_Read(&point) && !point.pressed);
    frame(2, 0, 100, 100);
    assert(FT6336U_Read(&point) && !point.pressed);
    frame(3, 0, 100, 100);
    assert(!FT6336U_Read(&point) && !point.pressed);
    frame(1, 3, 100, 100);
    assert(!FT6336U_Read(&point) && !point.pressed);
    frame(1, 0, 320, 100);
    assert(!FT6336U_Read(&point) && !point.pressed);
    frame(1, 0, 100, 480);
    assert(!FT6336U_Read(&point) && !point.pressed);
    /* 越界计数单独增加，便于实机区别“坐标范围猜错”和“I2C通信失败”。 */
    assert(g_ft6336u.out_of_range_count == 2 && g_ft6336u.last_event == 0);

    /* 正在按下时注入传输超时，必须松手、保留错误码且把锁还回去。 */
    frame(1, 0, 100, 200);
    assert(FT6336U_Read(&point) && point.pressed && point.x == 200 && point.y == 219);
    injected_status = HAL_TIMEOUT;
    hal_error = 0x20;
    assert(!FT6336U_Read(&point) && !point.pressed && !bus_mutex->held);
    assert(!g_ft6336u.pressed && g_ft6336u.last_status == HAL_TIMEOUT);
    assert(g_ft6336u.last_i2c_error == 0x20);
    injected_status = HAL_OK;
    hal_error = 0;
    assert(FT6336U_Read(&point) && point.pressed);

    /* 模拟OLED占用太久：不调用HAL，不误读其他任务的错误码。 */
    uint32_t old_transfers = transfers;
    deny_lock = 1;
    assert(!FT6336U_Read(&point) && !point.pressed);
    assert(transfers == old_transfers && g_ft6336u.last_status == HAL_BUSY);
    deny_lock = 0;
    assert(FT6336U_Read(&point));

    /* 三次连续失败后降为每秒重试；恢复后不重复拉复位，也不会永久卡死。 */
    injected_status = HAL_ERROR;
    for (unsigned i = 0; i < 3; ++i) assert(!FT6336U_Read(&point) && !point.pressed);
    assert(!g_ft6336u.ready && !bus_mutex->held);
    old_transfers = transfers;
    injected_status = HAL_OK;
    assert(!FT6336U_Read(&point) && transfers == old_transfers);
    now_ms += 1000;
    assert(FT6336U_Read(&point) && point.pressed && g_ft6336u.ready && reset_edges == 2);

    /* 地址应答但芯片ID错误必须拒绝；稍后ID恢复可自动重试成功。 */
    registers[0xA3] = 0x36;
    assert(!FT6336U_Init() && !g_ft6336u.ready && g_ft6336u.chip_id == 0x36);
    registers[0xA3] = 0x64;
    now_ms += 1000;
    assert(FT6336U_Read(&point) && point.pressed);
    isr = 1;
    old_transfers = transfers;
    assert(!FT6336U_Read(&point) && !point.pressed && transfers == old_transfers);
    isr = 0;
    assert(!FT6336U_Read(NULL) && !bus_mutex->held);
    puts("PASS: reset, bus ownership, chip ID, corners, events, errors and retry");
    return 0;
}
