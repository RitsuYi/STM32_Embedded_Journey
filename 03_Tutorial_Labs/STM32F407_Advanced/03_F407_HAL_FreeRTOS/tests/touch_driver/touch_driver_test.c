#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FT6336U.h"
#include "I2C2_Bus.h"
#include "main.h"
#include "i2c.h"
#include "task.h"
#include "semphr.h"

/* 测试失败仍必须返回非零退出码，供CTest判定失败；不能关闭检查来掩盖错误。
 * 这里不用标准assert触发abort，因为Windows可能因此弹出错误窗口并等待人工确认。
 * condition是失败条件的文字，file/line标出位置；先输出到标准错误流，再正常退出1。 */
static void test_check_failed(const char *condition, const char *file, int line)
{
    fprintf(stderr, "FAIL %s:%d: %s\n", file, line, condition);
    fflush(stderr);
    exit(EXIT_FAILURE);
}

/* 条件只计算一次，Release构建也始终执行；失败后停止，避免继续使用无效状态。 */
#define CHECK(condition) do { \
    if (!(condition)) test_check_failed(#condition, __FILE__, __LINE__); \
} while (0)

/* 用可控寄存器替代真实触摸芯片，重点验证错误不会变成按下、总线不会漏解锁。
 * 这些测试只验软件契约，不能证明I2C接线、实物方向或电容触摸性能。 */
GPIO_TypeDef test_port;
GPIO_TypeDef test_bus_port;
static I2C_TypeDef controller;
I2C_HandleTypeDef hi2c2 = {.Instance = &controller};
static uint8_t registers[256];
static uint32_t now_ms, isr, hal_error, transfers, reset_low_ms, reset_high_ms;
static unsigned reset_edges, delays, takes, gives, creates;
static int scheduler = taskSCHEDULER_RUNNING;
static int deny_lock;
static HAL_StatusTypeDef injected_status = HAL_OK;
static GPIO_PinState interrupt_level = GPIO_PIN_SET;
static SemaphoreHandle_t bus_mutex;
static bool fail_point_read; /* 单独注入第二笔坐标事务失败，检查整帧不会使用未收齐的数据。 */

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
    CHECK(mutex->held);
    mutex->held = 0;
    ++gives;
    return pdTRUE;
}
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *bus)
{
    CHECK(bus == &hi2c2 && bus_mutex->held);
    return hal_error;
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    if (port == GPIOB) return GPIO_PIN_SET; /* 总线恢复的故障波形由独立测试覆盖。 */
    CHECK(port == TP_INT_GPIO_Port && pin == TP_INT_Pin);
    return interrupt_level;
}
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
    if (port == GPIOB) { CHECK(bus_mutex->held); return; }
    CHECK(port == TP_RST_GPIO_Port && pin == TP_RST_Pin);
    CHECK(!bus_mutex->held); /* 复位等待不能霸占OLED总线。 */
    if (state == GPIO_PIN_RESET) reset_low_ms = now_ms;
    else reset_high_ms = now_ms;
    ++reset_edges;
}
/* 本套仍专注触摸报文；恢复时确认不丢失互斥，并保持线路空闲。
 * 另一个i2c2_recovery目标会实际模拟SDA/SCL卡低及控制器BUSY。 */
void HAL_GPIO_Init(GPIO_TypeDef *port, const GPIO_InitTypeDef *pins)
{
    CHECK(port == GPIOB && pins && bus_mutex->held);
}
void test_bus_reset(int asserted) { CHECK(bus_mutex->held); if (asserted) controller.SR2 = 0; }
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *bus)
{
    CHECK(bus == &hi2c2 && bus_mutex->held); return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *bus)
{
    CHECK(bus == &hi2c2 && bus_mutex->held); hal_error = 0; return HAL_OK;
}
static void check_transfer(I2C_HandleTypeDef *bus, uint16_t address,
                           uint16_t address_size, uint32_t timeout)
{
    CHECK(bus == &hi2c2 && address == 0x70U);
    CHECK(address_size == I2C_MEMADD_SIZE_8BIT && timeout == 20U);
    CHECK(bus_mutex->held && scheduler == taskSCHEDULER_RUNNING && !isr);
    /* 芯片复位释放后必须等待到手册要求的报告时间。 */
    CHECK(now_ms - reset_high_ms >= 300U);
    ++transfers;
}
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *bus, uint16_t address,
    uint16_t reg, uint16_t address_size, uint8_t *data, uint16_t size, uint32_t timeout)
{
    check_transfer(bus, address, address_size, timeout);
    /* 商家分开读点数(1字节)和坐标(4字节)，禁止再读13字节整块或无关寄存器。 */
    CHECK((reg == 2U && size == 1U) || (reg == 3U && size == 4U) ||
          (reg == 0xA3U && size == 1U));
    if (reg == 3U && fail_point_read) {
        hal_error = HAL_I2C_ERROR_TIMEOUT;
        return HAL_TIMEOUT;
    }
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
    /* 第二个点槽没有手指时使用无效事件/ID，不能让默认全零冒充另一根手指。 */
    memset(registers + 9, 0, 6);
    registers[9] = 0xC0;
    registers[11] = 0xF0;
}

/* 每个点槽有6字节，Y高字节的高4位是手指身份编号；槽顺序可变，编号才跟手。 */
static void slot(unsigned index, uint8_t id, uint8_t event, uint16_t x, uint16_t y)
{
    unsigned reg = 3U + index * 6U;
    registers[reg] = (uint8_t)((event << 6) | (x >> 8));
    registers[reg + 1U] = (uint8_t)x;
    registers[reg + 2U] = (uint8_t)((id << 4) | (y >> 8));
    registers[reg + 3U] = (uint8_t)y;
}

/* 只验证现在实际保留的商家读取契约；原中值/多指跟踪测试随功能撤回，不再用它们定义协议。 */
static void check_vendor_reports(void)
{
    FT6336U_Point point;
    /* 旧代码把DOWN偏点直接送入界面；商家要求CONTACT，故DOWN/UP均不按下。 */
    frame(1, 0, 0, 177);
    CHECK(FT6336U_Read(&point) && !point.pressed);
    frame(1, 2, 23, 177);
    unsigned old_takes = takes, old_gives = gives;
    CHECK(FT6336U_Read(&point) && point.pressed && point.x == 177 && point.y == 296);
    CHECK(takes == old_takes + 1U && gives == old_gives + 1U);
    fail_point_read = true;
    CHECK(!FT6336U_Read(&point) && !point.pressed && !bus_mutex->held);
    fail_point_read = false; hal_error = 0;
    CHECK(FT6336U_Read(&point) && point.pressed && point.y == 296);
    frame(1, 1, 0, 177);
    CHECK(FT6336U_Read(&point) && !point.pressed && point.x == 177 && point.y == 296);
    frame(1, 3, 20, 177);
    CHECK(FT6336U_Read(&point) && !point.pressed);
    frame(1, 2, 20, 177);
    registers[3] |= 0x10U; /* 商家检查整个高半字节，不只是最高两个事件位。 */
    CHECK(FT6336U_Read(&point) && !point.pressed);
    /* 不扫第二槽；N0/N2不读坐标，旧槽残留不造成按下。 */
    frame(2, 2, 20, 177);
    CHECK(FT6336U_Read(&point) && !point.pressed);
    frame(0, 2, 20, 177);
    slot(1, 0, 2, 10, 25);
    CHECK(FT6336U_Read(&point) && !point.pressed);
    frame(1, 1, 20, 177);
    slot(1, 0, 2, 10, 25);
    CHECK(FT6336U_Read(&point) && !point.pressed);
    /* 商家忽略双零，但单轴0仍是有效边缘；不能把它挪进某个按钮。 */
    frame(1, 2, 0, 0);
    CHECK(!FT6336U_Read(&point) && !point.pressed);
    frame(1, 2, 0, 177);
    CHECK(FT6336U_Read(&point) && point.pressed && point.x == 177 && point.y == 319);
    /* 320个合法原始X逐个直接响应，照片中的大变化也不再被软件历史改写。 */
    for (unsigned raw_x = 0; raw_x < 320U; ++raw_x) {
        frame(1, 2, raw_x, 240);
        CHECK(FT6336U_Read(&point) && point.pressed && point.y == 319U - raw_x);
    }
    const uint16_t photo_x[] = {16, 3, 51};
    const uint16_t expected_y[] = {303, 316, 268};
    for (unsigned i = 0; i < 3U; ++i) {
        frame(1, 2, photo_x[i], 433);
        CHECK(FT6336U_Read(&point) && point.pressed && point.y == expected_y[i]);
    }
    CHECK(registers[0] == 0xFF && registers[0xA4] == 0xFF);
}
int main(int argc, char **argv)
{
    /* 仅供失败报告自检：故意让同一CHECK失败。CMake会验证它迅速退出1并留下日志，
     * 防止以后又换回会触发Windows弹窗的退出方式；不执行任何触摸驱动测试。 */
    if (argc == 2 && strcmp(argv[1], "--self-test-failure") == 0) {
        CHECK(0 && "intentional failure reporting check");
    }
    CHECK(!I2C2_Bus_Lock(1));
    /* 回归检查：OLED在任何RTOS对象创建前就要完成启动通信和HAL延时。
     * 此时只有main执行，允许不建锁访问，且不能偷偷创建/取得RTOS锁。
     * 调度器运行以后则相反：未创建锁必须拒绝访问，不能绕过互斥。 */
    scheduler = taskSCHEDULER_NOT_STARTED;
    CHECK(I2C2_Bus_Lock(1));
    I2C2_Bus_Unlock();
    CHECK(creates == 0 && takes == 0 && gives == 0);
    isr = 1;
    CHECK(!I2C2_Bus_Lock(1));
    isr = 0;
    scheduler = taskSCHEDULER_RUNNING;
    CHECK(!I2C2_Bus_Lock(1));
    scheduler = taskSCHEDULER_NOT_STARTED;
    /* 对应main已完成OLED启动、即将创建其他RTOS对象的位置。 */
    CHECK(I2C2_Bus_Init() && I2C2_Bus_Init() && creates == 1);
    CHECK(I2C2_Bus_Lock(1));
    I2C2_Bus_Unlock();
    CHECK(takes == 0 && gives == 0);
    scheduler = taskSCHEDULER_SUSPENDED;
    CHECK(!I2C2_Bus_Lock(1));
    scheduler = taskSCHEDULER_RUNNING;
    isr = 1;
    CHECK(!I2C2_Bus_Lock(1) && !FT6336U_Init());
    isr = 0;
    CHECK(I2C2_Bus_Lock(1));
    CHECK(!I2C2_Bus_Lock(1));
    I2C2_Bus_Unlock();

    registers[0xA3] = 0x64;
    registers[0xA6] = 0x12;
    registers[0xA8] = 0x11;
    registers[0] = registers[0xA4] = 0xFF;
    CHECK(FT6336U_Init());
    CHECK(reset_edges == 2 && delays == 2 && reset_high_ms - reset_low_ms >= 20U);
    /* 初始化只读芯片ID；固件/厂商号属于已移除的调试查询。 */
    CHECK(transfers == 1U);
    CHECK(registers[0] == 0xFF && registers[0xA4] == 0xFF);

    /* 商家Init只有硬复位/等待，不写DEVICE_MODE或G_MODE；先对旧实现做差异回归。 */
    if (argc == 2 && strcmp(argv[1], "--vendor-contract") == 0) {
        CHECK(registers[0] == 0xFF && registers[0xA4] == 0xFF);
        frame(1, 0, 0, 177);
        FT6336U_Point first;
        CHECK(FT6336U_Read(&first) && !first.pressed);
        frame(1, 2, 23, 177);
        CHECK(FT6336U_Read(&first) && first.pressed && first.y == 296);
        puts("PASS: vendor reset defaults, DOWN ignored, CONTACT forwarded");
        return 0;
    }

    FT6336U_Point point;
    /* 用户实机确认屏幕Y需要反转；按独立端点表验证四角，防止修复时翻错轴。
     * 每行依次是原始X、原始Y、期望屏幕X、期望屏幕Y，不从配置宏计算期望值。 */
    const uint16_t corners[4][4] = {
        {0, 1, 1, 319}, {319, 0, 0, 0}, {0, 479, 479, 319}, {319, 479, 479, 0}
    };
    for (unsigned i = 0; i < 4; ++i) {
        frame(0, 1, 0, 0);
        CHECK(FT6336U_Read(&point) && !point.pressed);
        frame(1, 2, corners[i][0], corners[i][1]);
        CHECK(FT6336U_Read(&point) && point.pressed);
        CHECK(point.x == corners[i][2] && point.y == corners[i][3]);
    }
    /* 间隔变化及INT低电平不影响轮询松手，仍保留最后有效坐标。 */
    now_ms += 75;
    interrupt_level = GPIO_PIN_RESET;
    frame(0, 0, 0, 0);
    CHECK(FT6336U_Read(&point) && !point.pressed && point.x == 479 && point.y == 0);
    frame(1, 1, 0, 0);
    CHECK(FT6336U_Read(&point) && !point.pressed);
    frame(2, 0, 100, 100);
    CHECK(FT6336U_Read(&point) && !point.pressed);
    frame(3, 0, 100, 100);
    CHECK(!FT6336U_Read(&point) && !point.pressed);
    frame(1, 3, 100, 100);
    CHECK(FT6336U_Read(&point) && !point.pressed);
    frame(1, 2, 320, 100);
    CHECK(!FT6336U_Read(&point) && !point.pressed);
    frame(1, 2, 100, 480);
    CHECK(!FT6336U_Read(&point) && !point.pressed);

    /* 正在按下时注入传输超时，必须松手、保留最后坐标且把锁还回去。 */
    frame(1, 2, 100, 200);
    CHECK(FT6336U_Read(&point) && point.pressed && point.x == 200 && point.y == 219);
    injected_status = HAL_TIMEOUT;
    hal_error = 0x20;
    CHECK(!FT6336U_Read(&point) && !point.pressed && !bus_mutex->held);
    CHECK(point.x == 200 && point.y == 219);
    injected_status = HAL_OK;
    hal_error = 0;
    CHECK(FT6336U_Read(&point) && point.pressed);

    /* 模拟OLED占用太久：不调用HAL，不误读其他任务的错误码。 */
    uint32_t old_transfers = transfers;
    deny_lock = 1;
    CHECK(!FT6336U_Read(&point) && !point.pressed);
    CHECK(transfers == old_transfers);
    deny_lock = 0;
    CHECK(FT6336U_Read(&point));

    /* 三次连续失败后降为每秒重试；恢复后不重复拉复位，也不会永久卡死。 */
    injected_status = HAL_ERROR;
    for (unsigned i = 0; i < 3; ++i) CHECK(!FT6336U_Read(&point) && !point.pressed);
    CHECK(!bus_mutex->held);
    old_transfers = transfers;
    injected_status = HAL_OK;
    CHECK(!FT6336U_Read(&point) && transfers == old_transfers);
    now_ms += 1000;
    CHECK(FT6336U_Read(&point) && point.pressed && reset_edges == 2);

    /* 地址应答但芯片ID错误必须拒绝；稍后ID恢复可自动重试成功。 */
    registers[0xA3] = 0x36;
    CHECK(!FT6336U_Init());
    registers[0xA3] = 0x64;
    now_ms += 1000;
    CHECK(FT6336U_Read(&point) && point.pressed);
    check_vendor_reports();
    isr = 1;
    old_transfers = transfers;
    CHECK(!FT6336U_Read(&point) && !point.pressed && transfers == old_transfers);
    isr = 0;
    CHECK(!FT6336U_Read(NULL) && !bus_mutex->held);
    puts("PASS: reset, bus ownership, chip ID, corners, events, errors and retry");
    return 0;
}
