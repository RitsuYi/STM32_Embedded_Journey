#include "FT6336U.h"
#include "I2C2_Bus.h"
#include "ST7796.h"
#include "i2c.h"
#include "main.h"
#include "task.h"

/* 按商家示例ctp.c：先读0x02点数，再读0x03起的4字节坐标。
 * 只接受CONTACT事件，表示芯片已确认持续接触；不把初始DOWN当作有效坐标。
 * 保留本工程HAL/RTOS总线保护与已实测的横屏方向，不搬商家的F1引脚/软件I2C。 */
#define FT_REG_TOUCH_COUNT   0x02U
#define FT_REG_FIRST_POINT   0x03U
#define FT_REG_CHIP_ID       0xA3U
#define FT_EXPECTED_CHIP_ID  0x64U
#define FT_TRANSFER_MS       20U
#define FT_LOCK_MS           25U
#define FT_RETRY_MS          1000U

#if FT6336U_RAW_X_MAX <= FT6336U_RAW_X_MIN || FT6336U_RAW_Y_MAX <= FT6336U_RAW_Y_MIN
#error "FT6336U raw coordinate maximum must exceed minimum"
#endif

/* 这些是读取和重试所需的私有状态；松手/失败时保留最后有效坐标。
 * 只有GUI任务访问，不再发布供其他任务显示的调试快照。 */
static bool ready;
static uint16_t last_x, last_y;
static uint32_t last_probe_ms;
static uint8_t consecutive_errors;
/* 调用者已经取得总线锁。本函数读一个寄存器块并在锁内检查返回值，
 * 共用层必要时恢复控制器；失败仍返回false，不能把恢复当作读到了坐标。 */
static bool read_locked(uint8_t reg, uint8_t *data, uint16_t size)
{
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c2,
        FT6336U_I2C_ADDRESS_7BIT << 1, reg, I2C_MEMADD_SIZE_8BIT, data, size, FT_TRANSFER_MS);
    I2C2_Bus_ReportResult(FT6336U_I2C_ADDRESS_7BIT << 1, status);
    return status == HAL_OK;
}

static bool lock_bus(void)
{
    return I2C2_Bus_Lock(pdMS_TO_TICKS(FT_LOCK_MS) + 1U);
}

/* 身份查询只读寄存器，不改商家复位后的默认模式、扫描率或灵敏度。 */
static bool read_register(uint8_t reg, uint8_t *data)
{
    if (!lock_bus()) return false;
    bool valid = read_locked(reg, data, 1);
    I2C2_Bus_Unlock();
    return valid;
}

static bool configure(void)
{
    uint8_t value = 0;
    last_probe_ms = HAL_GetTick();
    ready = 0;
    if (!read_register(FT_REG_CHIP_ID, &value)) return false;
    /* 错芯片也可能应答地址；检查ID，不能只凭“有应答”就说初始化成功。 */
    if (value != FT_EXPECTED_CHIP_ID) return false;

    /* ID只用于确认芯片型号；保持商家硬复位后的默认模式，不读调试身份字段。 */
    consecutive_errors = 0;
    ready = 1;
    return true;
}

bool FT6336U_Init(void)
{
    if (__get_IPSR() != 0U || xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
        return false;
    ready = 0;
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
    point->x = last_x;
    point->y = last_y;
    point->pressed = false;
    if (__get_IPSR() != 0U || xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
        return false;
    /* 未接好或启动失败时，每秒只尝试一次；不反复复位、不死循环卡住GUI。
     * 共享I2C层只在HAL返回且仍持锁时做有限恢复，不打断OLED正在进行的事务。 */
    if (!ready &&
        (HAL_GetTick() - last_probe_ms < FT_RETRY_MS || !configure()))
        return false;

    /* 与商家一样分开读点数和第一个触点的4字节，不扫未使用的触点/权重。
     * 两次HAL事务持同一把锁，OLED不会插在点数与坐标之间。
     * 当前界面只支持一根手指；多指松手，避免另一个触点接管按钮。 */
    uint8_t count = 0, data[4];
    bool valid = false;
    if (lock_bus()) {
        valid = read_locked(FT_REG_TOUCH_COUNT, &count, 1);
        count &= 0x0FU;
        if (valid && count == 1U) valid = read_locked(FT_REG_FIRST_POINT, data, sizeof(data));
        I2C2_Bus_Unlock();
    }
    if (!valid) {
        if (++consecutive_errors >= 3U) {
            ready = 0;
            last_probe_ms = HAL_GetTick();
        }
        return false;
    }
    consecutive_errors = 0;
    if (count == 0U || count == 2U)
        return true;
    if (count > 2U) return false;
    uint16_t raw_x = (uint16_t)(((data[0] & 0x0FU) << 8) | data[1]);
    uint16_t raw_y = (uint16_t)(((data[2] & 0x0FU) << 8) | data[3]);
    /* 商家明确要求(XH & 0xF0)==0x80，即CONTACT；DOWN/UP不更新界面位置。
     * 非CONTACT仍按松手处理，并沿用最后有效坐标。 */
    if ((data[0] & 0xF0U) != 0x80U)
        return true;
    /* 商家也忽略两个原始坐标都为0的记录；单轴0是合法边缘，不能单独拒绝。 */
    if (raw_x == 0U && raw_y == 0U) return false;
    if (!map_point(raw_x, raw_y, point)) return false;
    /* 直接上报商家认可的CONTACT坐标，不再做中值、边缘补偿或历史猜点。 */
    point->pressed = true;
    last_x = point->x; last_y = point->y;
    return true;
}
