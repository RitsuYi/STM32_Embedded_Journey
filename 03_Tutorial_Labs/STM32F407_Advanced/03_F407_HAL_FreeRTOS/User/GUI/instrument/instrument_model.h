#ifndef INSTRUMENT_MODEL_H
#define INSTRUMENT_MODEL_H
#include "../measurement/measurement.h"

#define INSTRUMENT_RECORD_CAPACITY 64U

/* 公差用0.01mm整数保存。例如目标3000、下偏差10、上偏差20，
 * 允许区间就是29.90～30.20mm，端点也算合格，避免浮点比较误差。 */
typedef struct { uint32_t target_cmm, lower_cmm, upper_cmm; } InstrumentTolerance;
typedef enum { INSPECTION_NONE, INSPECTION_PASS, INSPECTION_PRIMARY,
               INSPECTION_SECONDARY, INSPECTION_INVALID } InstrumentVerdict;

typedef struct {
    MeasurementGeometry mode;
    uint32_t sample_count, quality_cmm, timeout_ms;
    bool inches, grid, dimensions, contour;
    uint32_t image_profile; /* 0: 640x480; 1: 1280x960 */
    InstrumentTolerance tolerance[2][2]; /* 第一维圆环/矩形，第二维外径/内径或长/宽。 */
} InstrumentSettings;
typedef struct {
    bool valid, simulated;
    uint32_t version, image_profile, reference_cmm, error_cmm;
    double homography[9]; /* pixel -> millimetre plane */
} InstrumentCalibration;
typedef struct {
    uint32_t id, calibration_version;
    MeasurementResult result;
    uint32_t uptime_ms; /* 开机后的测量时间；没有RTC时不能冒充真实日期。 */
    bool inspection;
    InstrumentVerdict verdict; /* 保存当时的判定，之后修改标准不重判历史。 */
} InstrumentRecord;
typedef struct {
    InstrumentSettings settings;
    InstrumentCalibration calibration;
    InstrumentRecord records[INSTRUMENT_RECORD_CAPACITY]; /* newest first */
    uint32_t record_count, next_id;
    uint32_t inspected, passed, failed; /* 本次内存模型累计值，不受64条记录滚动淘汰影响。 */
} InstrumentModel;
typedef struct { double x, y; } InstrumentPoint;

void instrument_defaults(InstrumentModel * model);
void instrument_default_settings(InstrumentSettings *settings);
bool instrument_settings_check(const InstrumentSettings * settings);
bool instrument_model_check(const InstrumentModel * model);
bool instrument_apply_settings(InstrumentModel * model, const InstrumentSettings * settings);
bool instrument_calibration_ready(const InstrumentModel * model);
bool instrument_calculate_calibration(const InstrumentPoint pixels[4], uint32_t reference_cmm,
                                      InstrumentPoint verification_pixel, uint32_t verification_cmm,
                                      uint32_t profile, InstrumentCalibration * candidate);
bool instrument_map_point(const InstrumentCalibration * calibration, InstrumentPoint pixel, InstrumentPoint * mm);
bool instrument_add_record(InstrumentModel * model, const MeasurementResult * result);
bool instrument_delete_record(InstrumentModel * model, uint32_t id);
/* 仅检查尺寸与配置标准；无效测量绝不能给出“合格”。 */
InstrumentVerdict instrument_inspect(const InstrumentSettings * settings, const MeasurementResult * result);
void instrument_format_dimension(char * buffer, size_t size, uint32_t cmm, bool inches);

/*
 * 根据已知矩形的四个角点计算长宽。
 * points_mm：按沿边顺序排列的四点，坐标单位为毫米。
 * length_cmm、width_cmm：用于接收结果，1代表0.01毫米。
 * 返回true表示成功；返回false时不修改输出。
 * 调用者须保证输入确实是矩形，本函数只负责尺寸计算。
 */
bool instrument_rectangle_dimensions(const InstrumentPoint points_mm[4],
                                     int32_t *length_cmm,
                                     int32_t *width_cmm);
                                     
#endif
