#include "FT6336U.h"
#include "I2C2_Bus.h"
#include "ST7796.h"
#include "i2c.h"
#include "main.h"
#include "task.h"

/* 寄存器是芯片内部可读写的小格子：0x02存手指数，0x03起存坐标与事件。
 * 依据FocalTech手册复位时序及Espressif官方FT6336U驱动的寄存器格式。
 * 链接与实测方法见User/GUI/README.md。 */
#define FT_REG_DEVICE_MODE   0x00U
#define FT_REG_TOUCH_COUNT   0x02U
#define FT_REG_CHIP_ID       0xA3U
#define FT_REG_G_MODE        0xA4U
#define FT_REG_FIRMWARE_ID   0xA6U
#define FT_REG_VENDOR_ID     0xA8U
#define FT_EXPECTED_CHIP_ID  0x64U
#define FT_TRANSFER_MS       20U
#define FT_LOCK_MS           25U
#define FT_RETRY_MS          1000U

#if FT6336U_RAW_X_MAX <= FT6336U_RAW_X_MIN || FT6336U_RAW_Y_MAX <= FT6336U_RAW_Y_MIN
#error "FT6336U raw coordinate maximum must exceed minimum"
#endif

volatile FT6336U_Diagnostics g_ft6336u;
static uint32_t last_probe_ms;
static uint8_t consecutive_errors;
/* 只由GUI任务写入，用来测量真正的采样间隔；第一次读取不计算启动前的等待。 */
static uint32_t last_read_ms;
static bool have_read_time;

/* 一次完整I2C传输只占一次锁。等待OLED发送完当前页即可读取触摸，
 * 不把锁跨越复位延时或整个OLED刷新。错误码在解锁前读取，避免被OLED覆盖。 */
static bool transfer(uint8_t reg, uint8_t *data, uint16_t size, bool write)
{
    HAL_StatusTypeDef status = HAL_BUSY;
    uint32_t error = HAL_I2C_ERROR_NONE;
    if (I2C2_Bus_Lock(pdMS_TO_TICKS(FT_LOCK_MS) + 1U)) {
        if (write)
            status = HAL_I2C_Mem_Write(&hi2c2, FT6336U_I2C_ADDRESS_7BIT << 1,
                        reg, I2C_MEMADD_SIZE_8BIT, data, size, FT_TRANSFER_MS);
        else
            status = HAL_I2C_Mem_Read(&hi2c2, FT6336U_I2C_ADDRESS_7BIT << 1,
                        reg, I2C_MEMADD_SIZE_8BIT, data, size, FT_TRANSFER_MS);
        error = HAL_I2C_GetError(&hi2c2);
        I2C2_Bus_Unlock();
    }
    g_ft6336u.last_status = status;
    g_ft6336u.last_i2c_error = error;
    if (status != HAL_OK) ++g_ft6336u.error_count;
    return status == HAL_OK;
}

static bool configure(void)
{
    uint8_t value = 0;
    ++g_ft6336u.init_count;
    last_probe_ms = HAL_GetTick();
    g_ft6336u.ready = 0;
    if (!transfer(FT_REG_CHIP_ID, &value, 1, false)) return false;
    g_ft6336u.chip_id = value;
    /* 错芯片也可能应答地址；检查ID，不能只凭“有应答”就说初始化成功。 */
    if (value != FT_EXPECTED_CHIP_ID) {
        g_ft6336u.last_status = HAL_ERROR;
        ++g_ft6336u.invalid_count;
        return false;
    }
    if (!transfer(FT_REG_FIRMWARE_ID, &value, 1, false)) return false;
    g_ft6336u.firmware_id = value;
    if (!transfer(FT_REG_VENDOR_ID, &value, 1, false)) return false;
    g_ft6336u.vendor_id = value;

    /* DEVICE_MODE=0进入普通工作模式；G_MODE=0选择轮询，GUI定时主动问状态。
     * 不改灵敏度/扫描率，也不进入工厂校准或休眠模式。 */
    value = 0;
    if (!transfer(FT_REG_DEVICE_MODE, &value, 1, true) ||
        !transfer(FT_REG_G_MODE, &value, 1, true)) return false;
    consecutive_errors = 0;
    g_ft6336u.ready = 1;
    return true;
}

bool FT6336U_Init(void)
{
    if (__get_IPSR() != 0U || xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
        return false;
    g_ft6336u.ready = 0;
    g_ft6336u.pressed = 0;
    have_read_time = false;
    g_ft6336u.read_gap_ms = 0;
    g_ft6336u.max_read_gap_ms = 0;
    /* 低电平复位至少5ms，释放后至少300ms才开始报告点。
     * 多加一个RTOS节拍，避免恰逢节拍边界使实际等待不足手册最小值。
     * vTaskDelay期间GUI任务休眠，OLED/按键等任务仍可运行。 */
    HAL_GPIO_WritePin(TP_RST_GPIO_Port, TP_RST_Pin, GPIO_PIN_RESET);
    vTaskDelay(pdMS_TO_TICKS(20) + 1U);
    HAL_GPIO_WritePin(TP_RST_GPIO_Port, TP_RST_Pin, GPIO_PIN_SET);
    vTaskDelay(pdMS_TO_TICKS(300) + 1U);
    return configure();
}

/* 先按原始端点换算为屏幕像素，再按需镜像；越界直接拒绝，避免边缘误点击。
 * 本模组先交换X/Y，再反转屏幕Y：(raw_x,raw_y)变为(raw_y,319-raw_x)。
 * 交换配合LCD横屏，上下反转则修正用户通过光标实测发现的触摸排线方向。
 * 乘法先转32位，防止中间结果超过16位；没有浮点运算或大块缓冲。 */
static bool map_point(uint16_t raw_x, uint16_t raw_y, FT6336U_Point *point)
{
    if (raw_x > FT6336U_RAW_X_MAX || raw_y > FT6336U_RAW_Y_MAX) return false;
#if FT6336U_RAW_X_MIN > 0
    if (raw_x < FT6336U_RAW_X_MIN) return false;
#endif
#if FT6336U_RAW_Y_MIN > 0
    if (raw_y < FT6336U_RAW_Y_MIN) return false;
#endif
    uint32_t x = raw_x - FT6336U_RAW_X_MIN;
    uint32_t y = raw_y - FT6336U_RAW_Y_MIN;
#if FT6336U_SWAP_XY
    point->x = (uint16_t)(y * (ST7796_WIDTH - 1U) /
                         (FT6336U_RAW_Y_MAX - FT6336U_RAW_Y_MIN));
    point->y = (uint16_t)(x * (ST7796_HEIGHT - 1U) /
                         (FT6336U_RAW_X_MAX - FT6336U_RAW_X_MIN));
#else
    point->x = (uint16_t)(x * (ST7796_WIDTH - 1U) /
                         (FT6336U_RAW_X_MAX - FT6336U_RAW_X_MIN));
    point->y = (uint16_t)(y * (ST7796_HEIGHT - 1U) /
                         (FT6336U_RAW_Y_MAX - FT6336U_RAW_Y_MIN));
#endif
#if FT6336U_INVERT_X
    point->x = ST7796_WIDTH - 1U - point->x;
#endif
#if FT6336U_INVERT_Y
    point->y = ST7796_HEIGHT - 1U - point->y;
#endif
    return true;
}

bool FT6336U_Read(FT6336U_Point *point)
{
    if (!point) return false;
    point->x = g_ft6336u.x;
    point->y = g_ft6336u.y;
    point->pressed = false;
    bool was_pressed = g_ft6336u.pressed != 0U;
    g_ft6336u.pressed = 0;
    g_ft6336u.point_count = 0;
    if (__get_IPSR() != 0U || xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
        return false;
    /* 记录实际进入读取的时间，不能把LVGL配置33ms当作硬件实测周期。
     * 用无符号相减也能跨越HAL毫秒计数回绕；本段不增加额外I2C访问。 */
    uint32_t now = HAL_GetTick();
    if (have_read_time) {
        g_ft6336u.read_gap_ms = now - last_read_ms;
        if (g_ft6336u.read_gap_ms > g_ft6336u.max_read_gap_ms)
            g_ft6336u.max_read_gap_ms = g_ft6336u.read_gap_ms;
    }
    last_read_ms = now;
    have_read_time = true;
    g_ft6336u.int_level = HAL_GPIO_ReadPin(TP_INT_GPIO_Port, TP_INT_Pin) == GPIO_PIN_SET;
    /* 未接好或启动失败时，每秒只尝试一次；不反复复位、不死循环卡住GUI。
     * 持续故障不自动重置共用I2C外设，以免破坏OLED正在进行的事务。 */
    if (!g_ft6336u.ready &&
        (HAL_GetTick() - last_probe_ms < FT_RETRY_MS || !configure())) return false;

    /* 一次读0x02~0x06五字节，点数与第一点坐标属于同一段传输。
     * XH/YH高四位含事件/手指编号，只有低四位属于12位坐标。 */
    uint8_t data[5];
    if (!transfer(FT_REG_TOUCH_COUNT, data, sizeof(data), false)) {
        if (++consecutive_errors >= 3U) {
            g_ft6336u.ready = 0;
            last_probe_ms = HAL_GetTick();
        }
        return false;
    }
    consecutive_errors = 0;
    ++g_ft6336u.read_count;
    uint8_t count = data[0] & 0x0FU;
    g_ft6336u.point_count = count;
    g_ft6336u.last_event = data[1] >> 6;
    if (count == 0U || count == 2U) return true;
    if (count > 2U) {
        ++g_ft6336u.invalid_count;
        return false;
    }
    uint8_t event = data[1] >> 6;
    if (event == 1U) return true; /* 1是抬起；该帧可能仍带旧坐标，不再按下。 */
    if (event == 3U) {
        ++g_ft6336u.invalid_count;
        return false;
    }
    uint16_t raw_x = (uint16_t)(((data[1] & 0x0FU) << 8) | data[2]);
    uint16_t raw_y = (uint16_t)(((data[3] & 0x0FU) << 8) | data[4]);
    g_ft6336u.raw_x = raw_x;
    g_ft6336u.raw_y = raw_y;
    if (!map_point(raw_x, raw_y, point)) {
        ++g_ft6336u.out_of_range_count;
        ++g_ft6336u.invalid_count;
        return false;
    }
    /* 事件0=刚按下，2=持续接触；只有坐标合法才提交给LVGL。 */
    point->pressed = true;
    g_ft6336u.x = point->x;
    g_ft6336u.y = point->y;
    g_ft6336u.pressed = 1;
    if (!was_pressed) ++g_ft6336u.press_count;
    return true;
}
